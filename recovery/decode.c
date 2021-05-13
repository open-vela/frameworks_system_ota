/****************************************************************************
 * services/recovery/decode.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#include <syslog.h>
#include "7zFile.h"
#include "7zVersion.h"
#include "Alloc.h"
#include "LzmaDec.h"

#include "velaimg.h"

#define IN_BUF_SIZE (1 << 16)
#define OUT_BUF_SIZE (1 << 16)

static SRes decode2(CLzmaDec* state, ISeqOutStream* outStream, ISeqInStream* inStream,
    UInt64 unpackSize)
{
    int thereIsSize = (unpackSize != (UInt64)(Int64)-1);
    Byte inBuf[IN_BUF_SIZE];
    Byte outBuf[OUT_BUF_SIZE];
    size_t inPos = 0, inSize = 0, outPos = 0;
    UInt64 totalSize = unpackSize;
    int percent = 0, oldpercent = 0;

    LzmaDec_Init(state);
    for (;;) {
        if (inPos == inSize) {
            inSize = IN_BUF_SIZE;
            RINOK(inStream->Read(inStream, inBuf, &inSize));
            inPos = 0;
        }
        {
            SRes ret;
            SizeT inProcessed = inSize - inPos;
            SizeT outProcessed = OUT_BUF_SIZE - outPos;
            ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
            ELzmaStatus status;
            if (thereIsSize && outProcessed > unpackSize) {
                outProcessed = (SizeT)unpackSize;
                finishMode = LZMA_FINISH_END;
            }

            ret = LzmaDec_DecodeToBuf(state, outBuf + outPos, &outProcessed,
                inBuf + inPos, &inProcessed, finishMode, &status);
            inPos += inProcessed;
            outPos += outProcessed;
            unpackSize -= outProcessed;

            if (outStream)
                if (outStream->Write(outStream, outBuf, outPos) != outPos)
                    return SZ_ERROR_WRITE;

            percent = (int)(100.0f * (totalSize - unpackSize) / totalSize);
            if (percent != oldpercent && percent % 10 == 0)
                syslog(LOG_INFO, "%d%% flashed\n", percent);
            oldpercent = percent;

            outPos = 0;

            if (ret != SZ_OK || (thereIsSize && unpackSize == 0))
                return ret;

            if (inProcessed == 0 && outProcessed == 0) {
                if (thereIsSize || status != LZMA_STATUS_FINISHED_WITH_MARK)
                    return SZ_ERROR_DATA;
                return ret;
            }
        }
    }
}

SRes decode(ISeqOutStream* outStream, ISeqInStream* inStream)
{
    UInt64 unpackSize;
    int i;
    SRes ret = 0;

    CLzmaDec state;

#ifdef CONFIG_LIB_MBEDTLS
    vela_img_hdr hdr;
    RINOK(SeqInStream_Read(inStream, &hdr, sizeof(hdr)));
#endif

    /* header: 5 bytes of LZMA properties and 8 bytes of uncompretsed size */
    unsigned char header[LZMA_PROPS_SIZE + 8];

    /* Read and parete header */

    RINOK(SeqInStream_Read(inStream, header, sizeof(header)));

    unpackSize = 0;
    for (i = 0; i < 8; i++)
        unpackSize += (UInt64)header[LZMA_PROPS_SIZE + i] << (i * 8);

    LzmaDec_Construct(&state);
    RINOK(LzmaDec_Allocate(&state, header, LZMA_PROPS_SIZE, &g_Alloc));
    ret = decode2(&state, outStream, inStream, unpackSize);
    LzmaDec_Free(&state, &g_Alloc);
    return ret;
}
