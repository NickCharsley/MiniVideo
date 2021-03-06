/*!
 * COPYRIGHT (C) 2010 Emeric Grange - All Rights Reserved
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
 * \file      h264.c
 * \author    Emeric Grange <emeric.grange@gmail.com>
 * \date      2010
 */

// C standard libraries
#include <stdio.h>
#include <stdlib.h>

// minivideo headers
#include "../../minitraces.h"
#include "../../typedef.h"
#include "h264_parameterset.h"
#include "h264_slice.h"
#include "h264_macroblock.h"

#include "h264.h"

/* ************************************************************************** */

static int computeNormAdjust(DecodingContext_t *dc);

/* ************************************************************************** */

/*!
 * \brief Decode H.264 bitstream.
 * \param *input_video A pointer to a VideoFile_t structure, containing every informations available about the current video file.
 * \param *output_directory The directory where to save exported pictures.
 * \param picture_format The picture file format.
 * \param picture_quality The quality we want for exported picture [1;100].
 * \param picture_number The number of thumbnail(s) we want to extract.
 * \param picture_extractionmode The method of distribution for thumbnails extraction.
 * \return 1 if succeed, 0 otherwise.
 *
 * This decoder is based on the 'ITU-T H.264' recommendation:
 * 'Advanced Video Coding for generic audiovisual services'
 * It also correspond to 'ISO/IEC 14496-10' international standard, part 10:
 * 'Advanced Video Coding'.
 *
 * You can download the H.264 specification for free on the ITU website:
 * http://www.itu.int/rec/T-REC-H.264
 *
 * The very first step to H.264 bitstream decoding. Initialize DecodingContext,
 * then start the decoding process, which loop on NAL Units found in the bitstream.
 * Each NAL Unit is processed following it's content type.
 */
int h264_decode(VideoFile_t *input_video,
                const char *output_directory,
                const int picture_format,
                const int picture_quality,
                const int picture_number,
                const int picture_extractionmode)
{
    TRACE_INFO(H264, BLD_GREEN "h264_decode()\n" CLR_RESET);
    int retcode = FAILURE;

    // Init decoding context
    DecodingContext_t *dc = initDecodingContext(input_video);

    if (dc == NULL)
    {
        TRACE_ERROR(H264, "Unable to allocate DecodingContext_t, exiting decoder\n");
        return retcode;
    }
    else
    {
        // Init some quantization parameters
        computeNormAdjust(dc);

        // Init some export parameters
        //strncpy(dc->output_directory, output_directory, sizeof(output_directory));
        dc->output_format = picture_format;
        dc->picture_quality = picture_quality;
        dc->picture_number = picture_number;
        dc->picture_extractionmode = picture_extractionmode;

        // Start the decoding process
        dc->decoderRunning = true;
    }

    // Loop until end of file
    while (dc->decoderRunning)
    {
        // Load next NAL Unit
        retcode = buffer_feed_next_sample(dc->bitstr);

        // Check header validity
        if (nalu_parse_header(dc->bitstr, dc->active_nalu))
        {
            // Decode NAL Unit content
            switch (dc->active_nalu->nal_unit_type)
            {
                case NALU_TYPE_SLICE: // 1
                {
                    TRACE_1(NALU, "This decoder only support IDR slice decoding!\n");
                }
                break;

                case NALU_TYPE_IDR: // 5
                {
                    dc->IdrPicFlag = true;
                    nalu_clean_sample(dc->bitstr);

                    TRACE_INFO(MAIN, "> " BLD_GREEN "decodeIDR(%i)\n" CLR_RESET, dc->idrCounter);

                    if (decode_slice(dc))
                    {
                        retcode = SUCCESS;
                        dc->errorCounter = 0;
                        dc->idrCounter++;
                        dc->frameCounter++;
                    }
                    else
                        dc->errorCounter++;

                    dc->IdrPicFlag = false;
                }
                break;

                case NALU_TYPE_SEI: // 6
                {
                    nalu_clean_sample(dc->bitstr);

                    if (decodeSEI(dc))
                    {
                        retcode = SUCCESS;
                        printSEI(dc);
                    }
                    else
                        dc->errorCounter++;
                }
                break;

                case NALU_TYPE_SPS: // 7
                {
                    nalu_clean_sample(dc->bitstr);

                    if (decodeSPS(dc))
                    {
                        retcode = SUCCESS;
                        printSPS(dc);
                    }
                    else
                        dc->errorCounter++;
                }
                break;

                case NALU_TYPE_PPS: // 8
                {
                    nalu_clean_sample(dc->bitstr);

                    if (decodePPS(dc))
                    {
                        retcode = SUCCESS;
                        printPPS(dc);
                    }
                    else
                        dc->errorCounter++;
                }
                break;

                default:
                {
                    dc->errorCounter++;
                    TRACE_ERROR(NALU, "Unsupported NAL Unit! (nal_unit_type %i)\n", dc->active_nalu->nal_unit_type);
                }
                break;
            }

            // Reset NAL Unit structure
            nalu_reset(dc->active_nalu);
        }
        else
        {
            dc->errorCounter++;
            TRACE_WARNING(NALU, "No valid NAL Unit to decode! (errorCounter = %i)\n", dc->errorCounter);
        }

        if (dc->idrCounter == picture_number)
        {
            TRACE_INFO(H264, ">> " BLD_YELLOW "Decoding of %i IDR successfull!\n" CLR_RESET, dc->idrCounter);
            TRACE_INFO(H264, "H.264 decoding ended\n");
            retcode = SUCCESS;
            dc->decoderRunning = false;
        }

        if (dc->errorCounter > 64 || retcode == FAILURE)
        {
            TRACE_ERROR(H264, "Error inside NAL Unit decoding loop! (errorCounter = %i) (current nal_unit_type = %i)\n", dc->errorCounter, dc->active_nalu->nal_unit_type);
            TRACE_ERROR(H264, "H.264 decoding aborted\n");
            retcode = FAILURE;
            dc->decoderRunning = false;
        }
    }

    // Destroy decoding context
    freeDecodingContext(&dc);

    // Exit decoder
    return retcode;
}

/* ************************************************************************** */
/* ************************************************************************** */

/*!
 * \brief Free decoding context.
 * \param *video A pointer to a VideoFile_t structure, containing every informations available about the current video file.
 * \return A pointer to the newlee allocated DecodingContext.
 *
 * Initialize the DecodingContext and it's bitstream (with a VideoFile_t passed in parameters),
 * then NAL Unit structure, then init all pointer's (sps, pps, sei) to NULL.
 */
DecodingContext_t *initDecodingContext(VideoFile_t *video)
{
    TRACE_INFO(H264, BLD_GREEN "initDecodingContext()\n" CLR_RESET);
    DecodingContext_t *dc = NULL;

    if (video != NULL)
    {
        // DecodingContext allocation
        dc = (DecodingContext_t*)calloc(1, sizeof(DecodingContext_t));

        if (dc == NULL)
        {
            TRACE_ERROR(H264, "Unable to alloc new DecodingContext!\n");
        }
        else
        {
            // Init output variables
            dc->output_format = 0;
            dc->picture_quality = 75;
            dc->picture_number = 1;
            dc->picture_extractionmode = 0;

            // Init input bitstream
            dc->VideoFile = video;
            dc->bitstr = init_bitstream(video, video->tracks_video[0]);

            if (dc->bitstr == NULL)
            {
                free(dc);
                dc = NULL;
            }
            else
            {
                // Init NAL Unit
                dc->active_nalu = initNALU();

                if (dc->active_nalu == NULL)
                {
                    free(dc->bitstr);
                    dc->bitstr = NULL;

                    free(dc);
                    dc = NULL;
                }
                else
                {
                    // Cleanup array of SPS and PPS
                    int i = 0;

                    for (i = 0; i < MAX_SPS; i++)
                    {
                        dc->sps_array[i] = NULL;
                    }

                    for (i = 0; i < MAX_PPS; i++)
                    {
                        dc->pps_array[i] = NULL;
                    }

                    dc->active_sei = NULL;

                    dc->active_slice = NULL;

                    dc->mb_array = NULL;

                    TRACE_1(H264, "* DecodingContext allocation success\n");
                }
            }
        }
    }

    // Return the DecodingContext
    return dc;
}

/* ************************************************************************** */

/*!
 * \param *dc The DecodingContext structure we want to check.
 * \return The error code.
 *
 * This function aim to the validity of DecodingContext.
 * If any of the following checks failed, we can assume that something prior to
 * this function call had gone wrong and that we cannot safely pursue decoding.
 */
int checkDecodingContext(DecodingContext_t *dc)
{
    TRACE_INFO(H264, "> " BLD_GREEN "checkDecodingContext()\n" CLR_RESET);
    int retcode = SUCCESS;

    if (dc->bitstr == NULL)
    {
        TRACE_WARNING(H264, "* Bitstream structure is invalid!\n");
        retcode = FAILURE;
    }

    if (dc->active_nalu == NULL)
    {
        TRACE_WARNING(H264, "* NAL Unit structure is invalid!\n");
        retcode = FAILURE;
    }

    if (dc->sps_array[dc->active_sps] == NULL)
    {
        TRACE_WARNING(H264, "* SPS structure currently in use is invalid!\n");
        retcode = FAILURE;
    }

    if (dc->pps_array[dc->active_pps] == NULL)
    {
        TRACE_WARNING(H264, "* PPS structure currently in use is invalid!\n");
        retcode = FAILURE;
    }

    if (dc->active_slice == NULL)
    {
        TRACE_WARNING(H264, "* Slice structure is invalid!\n");
        retcode = FAILURE;
    }
    else
    {
        pps_t *pps = dc->pps_array[dc->active_slice->pic_parameter_set_id];
        if (pps == NULL)
        {
            TRACE_WARNING(H264, "* The slice structure refer to an invalid PPS structure!\n");
            retcode = FAILURE;
        }
        else
        {
            sps_t *sps = dc->sps_array[pps->seq_parameter_set_id];
            if (sps == NULL)
            {
                TRACE_WARNING(H264, "* The slice structure refer to an invalid SPS structure!\n");
                retcode = FAILURE;
            }
        }
    }

    return retcode;
}

/* ************************************************************************** */

/*!
 * \brief Free decoding context.
 * \param **dc_ptr The decoding context to freed.
 *
 * Free the DecodingContext and it's attached content:
 * - Bitstream.
 * - Active NAL Unit.
 * - All existing SPS.
 * - All existing PPS.
 * - Active SEI.
 * - Active Slice.
 * - All existing macroblocks.
 *
 * The VideoFile_t is not freed.
 */
void freeDecodingContext(DecodingContext_t **dc_ptr)
{
    TRACE_INFO(H264, BLD_GREEN "freeDecodingContext()\n" CLR_RESET);

    // free DecodingContext content
    if ((*dc_ptr) != NULL)
    {
        int i = 0;
        free_bitstream(&(*dc_ptr)->bitstr);

        if ((*dc_ptr)->active_nalu != NULL)
        {
            free((*dc_ptr)->active_nalu);
            (*dc_ptr)->active_nalu = NULL;

            TRACE_1(H264, ">> NAL Unit freed\n");
        }

        for (i = 0; i < MAX_SPS; i++)
        {
            freeSPS(&(*dc_ptr)->sps_array[i]);
        }

        for (i = 0; i < MAX_PPS; i++)
        {
            freePPS(&(*dc_ptr)->pps_array[i]);
        }

        freeSEI(&(*dc_ptr)->active_sei);

        free_slice(&(*dc_ptr)->active_slice);

        freeMbArray(*dc_ptr);

        {
            free(*dc_ptr);
            *dc_ptr = NULL;

            TRACE_1(H264, ">> DecodingContext freed\n");
        }
    }
}

/* ************************************************************************** */
/* ************************************************************************** */

/*!
 * \param *dc The current DecodingContext.
 * \return 0 if failed, 1 otherwise.
 *
 * Compute some values needed during the quantization of the coefficients
 * contained in the bitstream.
 */
static int computeNormAdjust(DecodingContext_t *dc)
{
    TRACE_2(TRANS, "  > " BLD_GREEN "computeNormAdjust()\n" CLR_RESET);
    int retcode = FAILURE;

    if (dc != NULL)
    {
        int q = 0, i = 0, j = 0;

        int v4x4[6][3] =
        {
            {10, 16, 13},
            {11, 18, 14},
            {13, 20, 16},
            {14, 23, 18},
            {16, 25, 20},
            {18, 29, 23},
        };

        int v8x8[6][6] =
        {
            {20, 18, 32, 19, 25, 24},
            {22, 19, 35, 21, 28, 26},
            {26, 23, 42, 24, 33, 31},
            {28, 25, 45, 26, 35, 33},
            {32, 28, 51, 30, 40, 38},
            {36, 32, 58, 34, 46, 43},
        };

        for (q = 0; q < 6; q++)
        {
            // Compute complete normAdjust4x4[] table
            for (i = 0; i < 4; i++)
            {
                for (j = 0; j < 4; j++)
                {
                    if ((i % 2 == 0) && (j % 2 == 0))
                        dc->normAdjust4x4[q][i][j] = v4x4[q][0];
                    else if ((i % 2 == 1) && (j % 2 == 1))
                        dc->normAdjust4x4[q][i][j] = v4x4[q][1];
                    else
                        dc->normAdjust4x4[q][i][j] = v4x4[q][2];

                    //TRACE_3(DTRANS, "normAdjust4x4[%i][%i][%i] = %i\n", q, ii, jj, normAdjust4x4[q][ii][jj]);
                }
            }

            // Compute complete normAdjust8x8[] table
            for (i = 0; i < 8; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if ((i % 4 == 0) && (j % 4 == 0))
                        dc->normAdjust8x8[q][i][j] = v8x8[q][0];
                    else if ((i % 2 ==  1) && (j % 2 == 1))
                        dc->normAdjust8x8[q][i][j] = v8x8[q][1];
                    else if ((i % 4 == 2) && (j % 4 == 2))
                        dc->normAdjust8x8[q][i][j] = v8x8[q][2];
                    else if (((i % 4 == 0) && (j % 2 == 1)) || ((i % 2 == 1) && (j % 4 == 0)))
                        dc->normAdjust8x8[q][i][j] = v8x8[q][3];
                    else if (((i % 4 == 0) && (j % 4 == 2)) || ((i % 4 == 2) && (j % 4 == 0)))
                        dc->normAdjust8x8[q][i][j] = v8x8[q][4];
                    else
                        dc->normAdjust8x8[q][i][j] = v8x8[q][5];

                    //TRACE_3(DTRANS, "normAdjust8x8[%i][%i][%i] = %i\n", q, ii, jj, normAdjust8x8[q][ii][jj]);
                }
            }
        }

        retcode = SUCCESS;
    }

    return retcode;
}

/* ************************************************************************** */
