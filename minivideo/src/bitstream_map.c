/*!
 * COPYRIGHT (C) 2012 Emeric Grange - All Rights Reserved
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
 * \file      bitstream_map.c
 * \author    Emeric Grange <emeric.grange@gmail.com>
 * \date      2012
 */

// C standard libraries
#include <stdlib.h>

// minivideo headers
#include "bitstream.h"
#include "minitraces.h"
#include "avcodecs.h"

/* ************************************************************************** */

/*!
 * \brief Initialize a bitstream_map structure with a fixed number of entries.
 * \param bitstream_map A pointer to the *bitstreamMap_t structure to initialize.
 * \param entries The number of sample to init into the bitstreamMap_t structure.
 * \return retcode 1 if succeed, 0 otherwise.
 *
 * Everything inside the bitstreamMap_t structure is set to 0, even the number
 * of entries (sample_count).
 */
int init_bitstream_map(BitstreamMap_t **bitstream_map, uint32_t entries)
{
    TRACE_INFO(DEMUX, "<b> " BLD_BLUE "init_bitstream_map()\n" CLR_RESET);
    int retcode = SUCCESS;

    if (*bitstream_map != NULL)
    {
        TRACE_ERROR(DEMUX, "<b> Unable to alloc a new bitstream_map: not null!\n");
        retcode = FAILURE;
    }
    else
    {
        if (entries == 0)
        {
            TRACE_ERROR(DEMUX, "<b> Unable to allocate a new bitstream_map: no entries to allocate!\n");
            retcode = FAILURE;
        }
        else
        {
            *bitstream_map = (BitstreamMap_t*)calloc(1, sizeof(BitstreamMap_t));

            if (*bitstream_map == NULL)
            {
                TRACE_ERROR(DEMUX, "<b> Unable to allocate a new bitstream_map!\n");
                retcode = FAILURE;
            }
            else
            {
                (*bitstream_map)->sample_type = (uint32_t*)calloc(entries, sizeof(uint32_t));
                (*bitstream_map)->sample_size = (uint32_t*)calloc(entries, sizeof(uint32_t));
                (*bitstream_map)->sample_offset = (int64_t*)calloc(entries, sizeof(int64_t));
                (*bitstream_map)->sample_pts = (int64_t*)calloc(entries, sizeof(int64_t));
                (*bitstream_map)->sample_dts = (int64_t*)calloc(entries, sizeof(int64_t));

                if ((*bitstream_map)->sample_type == NULL ||
                    (*bitstream_map)->sample_size == NULL ||
                    (*bitstream_map)->sample_offset == NULL ||
                    (*bitstream_map)->sample_pts == NULL ||
                    (*bitstream_map)->sample_dts == NULL)
                {
                    TRACE_ERROR(DEMUX, "<b> Unable to alloc bitstream_map > sample_type / sample_size / sample_offset / sample_timecode!\n");

                    if ((*bitstream_map)->sample_type != NULL)
                        free((*bitstream_map)->sample_type);
                    if ((*bitstream_map)->sample_size != NULL)
                        free((*bitstream_map)->sample_size);
                    if ((*bitstream_map)->sample_offset != NULL)
                        free((*bitstream_map)->sample_offset);
                    if ((*bitstream_map)->sample_pts != NULL)
                        free((*bitstream_map)->sample_pts);
                    if ((*bitstream_map)->sample_dts != NULL)
                        free((*bitstream_map)->sample_dts);

                    free(*bitstream_map);
                    *bitstream_map = NULL;
                    retcode = FAILURE;
                }
            }
        }
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Destroy a bitstream_map structure.
 * \param *bitstream_map A pointer to a *bitstreamMap_t structure.
 */
void free_bitstream_map(BitstreamMap_t **bitstream_map)
{
    if ((*bitstream_map) != NULL)
    {
        TRACE_INFO(DEMUX, "<b> " BLD_BLUE "free_bitstream_map()\n" CLR_RESET);

        if ((*bitstream_map)->sample_type != NULL)
        {
            free((*bitstream_map)->sample_type);
        }

        if ((*bitstream_map)->sample_size != NULL)
        {
            free((*bitstream_map)->sample_size);
        }

        if ((*bitstream_map)->sample_offset != NULL)
        {
            free((*bitstream_map)->sample_offset);
        }

        if ((*bitstream_map)->sample_pts != NULL)
        {
            free((*bitstream_map)->sample_pts);
        }

        if ((*bitstream_map)->sample_dts != NULL)
        {
            free((*bitstream_map)->sample_dts);
        }

        free(*bitstream_map);
        *bitstream_map = NULL;

        TRACE_1(DEMUX, "<b> Bitstream_map freed\n");
    }
}

/* ************************************************************************** */
/* ************************************************************************** */

/*!
 * \brief Print the content of a bitstreamMap_t structure.
 * \param bitstream_map docme.
 */
void print_bitstream_map(BitstreamMap_t *bitstream_map)
{
#if ENABLE_DEBUG

    if (bitstream_map == NULL)
    {
        TRACE_ERROR(DEMUX, "Invalid bitstream_map structure!\n");
    }
    else
    {
        TRACE_INFO(DEMUX, BLD_GREEN "print_bitstream_map()\n" CLR_RESET);

        if (bitstream_map->stream_type == stream_VIDEO &&
            bitstream_map->sample_count > 0)
        {
            TRACE_INFO(DEMUX, "Elementary stream type > VIDEO\n");
        }
        else if (bitstream_map->stream_type == stream_AUDIO &&
                 bitstream_map->sample_count > 0)
        {
            TRACE_INFO(DEMUX, "Elementary stream type > AUDIO\n");
        }
        else
        {
            TRACE_WARNING(DEMUX, "Unknown elementary stream type!\n");
        }

        TRACE_1(DEMUX, "Track codec:     '%s'\n", getCodecString(bitstream_map->stream_type, bitstream_map->stream_codec));

        TRACE_INFO(DEMUX, "> samples alignment: %i\n", bitstream_map->sample_alignment);
        TRACE_INFO(DEMUX, "> samples count    : %i\n", bitstream_map->sample_count);
        TRACE_INFO(DEMUX, "> IDR samples count: %i\n", bitstream_map->sample_count_idr);

        if (bitstream_map->sample_count > 0)
        {
            TRACE_1(DEMUX, "SAMPLES\n");
            unsigned int i = 0;
            for (i = 0; i < bitstream_map->sample_count; i++)
            {
                TRACE_1(DEMUX, "> sample_type      : %i\n", bitstream_map->sample_type[i]);
                TRACE_1(DEMUX, "  | sample_offset  : %i\n", bitstream_map->sample_offset[i]);
                TRACE_1(DEMUX, "  | sample_size    : %i\n", bitstream_map->sample_size[i]);
                TRACE_1(DEMUX, "  | sample_timecode: %i\n", bitstream_map->sample_pts[i]);
            }
        }
    }

#endif /* ENABLE_DEBUG */
}

/* ************************************************************************** */
