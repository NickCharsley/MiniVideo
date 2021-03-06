/*!
 * COPYRIGHT (C) 2011 Emeric Grange - All Rights Reserved
 *
 * This file is part of MiniVideo.
 *
 * MiniVideo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MiniVideo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with MiniVideo.  If not, see <http://www.gnu.org/licenses/>.
 *
 * \file      filter.c
 * \author    Emeric Grange <emeric.grange@gmail.com>
 * \date      2011
 */

// C standard library
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// minivideo headers
#include "../typedef.h"
#include "../minitraces.h"

#include "filter.h"
#include "../import.h"
#include "../bitstream_map.h"

/* ************************************************************************** */

/*!
 * \brief Depending on picture_extractionmode parameter, choose some IDR from the bitstreamMap_t structure and delete the others.
 * \param **bitstream_map_pointer: docme.
 * \param picture_number: The number of thumbnail(s) we want to extract.
 * \param picture_extraction_mode: The method of distribution for thumbnails extraction.
 * \return The number of picture available in the bitstream map (0 means error).
 *
 * The IDR filter aim to remove irrelevant frames from the decode stream. By irrelevant we mean:
 * - Unicolor images (black or green screen) like the very first or very last frames of a stream.
 * - Images carrying less visual informations than the average.
 * - If specified, the filter select images spread over the duration of the film.
 */
int idr_filtering(BitstreamMap_t **bitstream_map_pointer,
                  int picture_number, const int picture_extraction_mode)
{
    TRACE_INFO(FILTER, BLD_GREEN "idr_filtering()\n" CLR_RESET);
    int retcode = FAILURE;

    int i = 0;
    int temporary_totalsamples_idr = 0;
    int temporary_sample_id[999] = {0};

    if (bitstream_map_pointer == NULL || *bitstream_map_pointer == NULL)
    {
        TRACE_ERROR(FILTER, "Invalid bitstream_map structure!\n");
    }
    else
    {
        BitstreamMap_t *map = *bitstream_map_pointer;

        // Check if the bitstream_map is containing video data
        if (map->stream_type != stream_VIDEO)
        {
            TRACE_WARNING(FILTER, "This is not an video bitstream_map!\n");
        }

        // Check if we have enough IDR samples inside the video
        if (map->sample_count_idr == 0)
        {
            TRACE_WARNING(FILTER, "No IDR samples inside the stream, 0 pictures will be extracted!\n", map->sample_count_idr);
            picture_number = 0;
        }
        else if (map->sample_count_idr < picture_number)
        {
            TRACE_WARNING(FILTER, "Not enough IDR samples inside the stream, only %i pictures will be extracted!\n", map->sample_count_idr);
            picture_number = map->sample_count_idr;
        }

        if (picture_extraction_mode == PICTURE_UNFILTERED)
        {
            TRACE_1(FILTER, "PICTURE_UNFILTERED is specified: no need to process bitstream_map.\n");
            retcode = picture_number;
        }
        else
        {
            // Warning: this is not true anymore, must count that manually
            int spspps = map->sample_count - map->sample_count_idr;
            int payload = 0;

            // Compute average samples size
            for (i = spspps; i < map->sample_count; i++)
            {
                payload += map->sample_size[i];
            }

            // Used to filter the frames that are below the threshold (33% of the average frame size)
            int frame_sizethreshold = (int)(((double)payload / (double)map->sample_count_idr) / 1.66);

            // Used to filter the frames from the first and last 3%
            // Note: for a movie, cut the last 33% to avoid spoilers & credits)
            int frame_borders = (int)ceil(map->sample_count_idr * 0.03);

            // First cut
            for (i = frame_borders; i < (map->sample_count_idr - frame_borders); i++)
            {
                TRACE_1(FILTER, "IDR %i (size: %i / threshold: %i)\n", i, map->sample_size[i + spspps], frame_sizethreshold);

                if (map->sample_size[i + spspps] > frame_sizethreshold)
                {
                    temporary_sample_id[temporary_totalsamples_idr] = i + spspps;
                    temporary_totalsamples_idr++;
                }
            }

            TRACE_1(FILTER, "We have a total of %i IDR after the first cut\n", temporary_totalsamples_idr);
            if (picture_number > temporary_totalsamples_idr)
                picture_number = temporary_totalsamples_idr;

            // Jump between two frames in PICTURE_DISTRIBUTED mode
            int frame_jump = (int)ceil(temporary_totalsamples_idr / (picture_number-1));

            // Write bitstream_map_filtered
            //(*bitstream_map_filtered) = (bitstreamMap_t*)calloc(1, sizeof(bitstreamMap_t));

            // Init bitstream_map_filtered
            BitstreamMap_t *map_filtered = NULL;
            retcode = init_bitstream_map(&map_filtered, spspps + temporary_totalsamples_idr);

            // Write bitstream_map_filtered
            if (retcode)
            {
                map_filtered->sample_count = spspps + temporary_totalsamples_idr;
                map_filtered->sample_count_idr = temporary_totalsamples_idr;

                // Copy SPS and PPS
                for (i = 0; i < spspps; i++)
                {
                    map_filtered->sample_type[i] = map->sample_type[i];
                    map_filtered->sample_pts[i] = map->sample_pts[i];

                    map_filtered->sample_offset[i] = map->sample_offset[i];
                    map_filtered->sample_size[i] = map->sample_size[i];
                }

                // Set idr (second cut)
                for (i = 0; i < picture_number; i++)
                {
                    if (picture_extraction_mode == PICTURE_ORDERED)
                    {
                        map_filtered->sample_type[spspps + i] = map->sample_type[temporary_sample_id[i]];
                        map_filtered->sample_pts[spspps + i] = map->sample_pts[temporary_sample_id[i]];

                        map_filtered->sample_offset[spspps + i] = map->sample_offset[temporary_sample_id[i]];
                        map_filtered->sample_size[spspps + i] = map->sample_size[temporary_sample_id[i]];
                    }
                    else if (picture_extraction_mode == PICTURE_DISTRIBUTED)
                    {
                        map_filtered->sample_type[spspps + i] = map->sample_type[temporary_sample_id[i*frame_jump]];
                        map_filtered->sample_pts[spspps + i] = map->sample_pts[temporary_sample_id[i*frame_jump]];

                        map_filtered->sample_offset[spspps + i] = map->sample_offset[temporary_sample_id[i*frame_jump]];
                        map_filtered->sample_size[spspps + i] = map->sample_size[temporary_sample_id[i*frame_jump]];
                    }
                }

                // Recap
                print_bitstream_map(map);
                print_bitstream_map(map_filtered);

                // Erase bitstream_map and replace it with bitstream_map_filtered
                free_bitstream_map(bitstream_map_pointer);
                bitstream_map_pointer = &map_filtered;

                // Exit
                retcode = picture_number;
            }
        }
    }

    return retcode;
}

/* ************************************************************************** */
