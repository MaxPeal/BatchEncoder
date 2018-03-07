﻿// Copyright (c) Wiesław Šoltés. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "stdafx.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace BatchEncoderCoreUnitTests
{
    TEST_CLASS(CCommandLine_Tests)
    {
        worker::Win32FileSystem FileSystem;
        config::CFormat m_Format
        {
            L"TEST_ID",
            L"Name",
            L"$EXE $OPTIONS $INFILE $OUTFILE",
            false,
            false,
            L"script.lua",
            L"program.exe",
            0,
            config::FormatType::Encoder,
            0,
            L"WAV",
            L"MP3",
            0,
            {
                { L"Default1", L"--option 1" },
                { L"Default2", L"--option 2" }
            }
        };
        config::CItem m_ltItem
        {
            0,
            {
                { L"C:\\Output\\File.WAV" }
            },
            1024,
            L"File",
            L"WAV",
            L"TEST_ID",
            0,
            L"",
            true,
            L"", L"", false, 0, 0
        };
    public:
        TEST_METHOD(CCommandLine_Build_Default)
        {
            config::CFormat format = m_Format;
            config::CItem item = m_ltItem;

            std::wstring szInputFile = L"C:\\Output\\File.WAV";
            std::wstring szOutputFile = L"C:\\Output\\File.MP3";

            worker::CCommandLine cl(&FileSystem, format, item.nPreset, item.nId, szInputFile, szOutputFile, item.szOptions);

            Assert::AreEqual(format.szId, cl.format.szId);
            Assert::AreEqual(item.nPreset, cl.nPreset);
            Assert::AreEqual(szInputFile, cl.szInputFile);
            Assert::AreEqual(szOutputFile, cl.szOutputFile);
            Assert::AreEqual(format.bPipeInput, cl.bUseReadPipes);
            Assert::AreEqual(format.bPipeOutput, cl.bUseWritePipes);
            Assert::AreEqual(format.m_Presets[item.nPreset].szOptions, cl.szOptions);
            Assert::AreEqual(L"C:\\Output\\", cl.szOutputPath.c_str());

            std::wstring szCommandLine = L"\"program.exe\" --option 1 \"C:\\Output\\File.WAV\" \"C:\\Output\\File.MP3\"";
            Assert::AreEqual(szCommandLine.c_str(), cl.szCommandLine.c_str());
        }

        TEST_METHOD(CCommandLine_Build_AdditionalOptions)
        {
            config::CFormat format = m_Format;
            config::CItem item = m_ltItem;

            std::wstring szInputFile = L"C:\\Output\\File.WAV";
            std::wstring szOutputFile = L"C:\\Output\\File.MP3";

            item.szOptions = L"--test";

            worker::CCommandLine cl(&FileSystem, format, item.nPreset, item.nId, szInputFile, szOutputFile, item.szOptions);

            Assert::AreEqual(format.szId, cl.format.szId);
            Assert::AreEqual(item.nPreset, cl.nPreset);
            Assert::AreEqual(szInputFile, cl.szInputFile);
            Assert::AreEqual(szOutputFile, cl.szOutputFile);
            Assert::AreEqual(format.bPipeInput, cl.bUseReadPipes);
            Assert::AreEqual(format.bPipeOutput, cl.bUseWritePipes);

            std::wstring szOptions = format.m_Presets[item.nPreset].szOptions + L" " + item.szOptions;
            Assert::AreEqual(szOptions, cl.szOptions);

            Assert::AreEqual(L"C:\\Output\\", cl.szOutputPath.c_str());

            std::wstring szCommandLine = L"\"program.exe\" --option 1 --test \"C:\\Output\\File.WAV\" \"C:\\Output\\File.MP3\"";
            Assert::AreEqual(szCommandLine.c_str(), cl.szCommandLine.c_str());
        }

        TEST_METHOD(CCommandLine_Build_OutPath)
        {
            config::CFormat format = m_Format;
            config::CItem item = m_ltItem;

            std::wstring szInputFile = L"C:\\Output\\File.WAV";
            std::wstring szOutputFile = L"C:\\Output\\File.MP3";

            format.szTemplate = L"$EXE $OPTIONS $INFILE $OUTFILE $OUTPATH";

            worker::CCommandLine cl(&FileSystem, format, item.nPreset, item.nId, szInputFile, szOutputFile, item.szOptions);

            Assert::AreEqual(format.szId, cl.format.szId);
            Assert::AreEqual(item.nPreset, cl.nPreset);
            Assert::AreEqual(szInputFile, cl.szInputFile);
            Assert::AreEqual(szOutputFile, cl.szOutputFile);
            Assert::AreEqual(format.bPipeInput, cl.bUseReadPipes);
            Assert::AreEqual(format.bPipeOutput, cl.bUseWritePipes);
            Assert::AreEqual(format.m_Presets[item.nPreset].szOptions, cl.szOptions);
            Assert::AreEqual(L"C:\\Output\\", cl.szOutputPath.c_str());

            std::wstring szCommandLine = L"\"program.exe\" --option 1 \"C:\\Output\\File.WAV\" \"C:\\Output\\File.MP3\" \"C:\\Output\\\"";
            Assert::AreEqual(szCommandLine.c_str(), cl.szCommandLine.c_str());
        }

        TEST_METHOD(CCommandLine_Build_UseReadPipes)
        {
            config::CFormat format = m_Format;
            config::CItem item = m_ltItem;

            format.bPipeInput = true;
            format.bPipeOutput = false;

            std::wstring szInputFile = L"C:\\Output\\File.WAV";
            std::wstring szOutputFile = L"C:\\Output\\File.MP3";

            worker::CCommandLine cl(&FileSystem, format, item.nPreset, item.nId, szInputFile, szOutputFile, item.szOptions);

            Assert::AreEqual(format.szId, cl.format.szId);
            Assert::AreEqual(item.nPreset, cl.nPreset);
            Assert::AreEqual(szInputFile, cl.szInputFile);
            Assert::AreEqual(szOutputFile, cl.szOutputFile);
            Assert::AreEqual(format.bPipeInput, cl.bUseReadPipes);
            Assert::AreEqual(format.bPipeOutput, cl.bUseWritePipes);
            Assert::AreEqual(format.m_Presets[item.nPreset].szOptions, cl.szOptions);
            Assert::AreEqual(L"C:\\Output\\", cl.szOutputPath.c_str());

            std::wstring szCommandLine = L"\"program.exe\" --option 1 \"-\" \"C:\\Output\\File.MP3\"";
            Assert::AreEqual(szCommandLine.c_str(), cl.szCommandLine.c_str());
        }

        TEST_METHOD(CCommandLine_Build_UseWritePipes)
        {
            config::CFormat format = m_Format;
            config::CItem item = m_ltItem;

            format.bPipeInput = false;
            format.bPipeOutput = true;

            std::wstring szInputFile = L"C:\\Output\\File.WAV";
            std::wstring szOutputFile = L"C:\\Output\\File.MP3";

            worker::CCommandLine cl(&FileSystem, format, item.nPreset, item.nId, szInputFile, szOutputFile, item.szOptions);

            Assert::AreEqual(format.szId, cl.format.szId);
            Assert::AreEqual(item.nPreset, cl.nPreset);
            Assert::AreEqual(szInputFile, cl.szInputFile);
            Assert::AreEqual(szOutputFile, cl.szOutputFile);
            Assert::AreEqual(format.bPipeInput, cl.bUseReadPipes);
            Assert::AreEqual(format.bPipeOutput, cl.bUseWritePipes);
            Assert::AreEqual(format.m_Presets[item.nPreset].szOptions, cl.szOptions);
            Assert::AreEqual(L"C:\\Output\\", cl.szOutputPath.c_str());

            std::wstring szCommandLine = L"\"program.exe\" --option 1 \"C:\\Output\\File.WAV\" \"-\"";
            Assert::AreEqual(szCommandLine.c_str(), cl.szCommandLine.c_str());
        }

        TEST_METHOD(CCommandLine_Build_UseReadWritePipes)
        {
            config::CFormat format = m_Format;
            config::CItem item = m_ltItem;

            format.bPipeInput = true;
            format.bPipeOutput = true;

            std::wstring szInputFile = L"C:\\Output\\File.WAV";
            std::wstring szOutputFile = L"C:\\Output\\File.MP3";

            worker::CCommandLine cl(&FileSystem, format, item.nPreset, item.nId, szInputFile, szOutputFile, item.szOptions);

            Assert::AreEqual(format.szId, cl.format.szId);
            Assert::AreEqual(item.nPreset, cl.nPreset);
            Assert::AreEqual(szInputFile, cl.szInputFile);
            Assert::AreEqual(szOutputFile, cl.szOutputFile);
            Assert::AreEqual(format.bPipeInput, cl.bUseReadPipes);
            Assert::AreEqual(format.bPipeOutput, cl.bUseWritePipes);
            Assert::AreEqual(format.m_Presets[item.nPreset].szOptions, cl.szOptions);
            Assert::AreEqual(L"C:\\Output\\", cl.szOutputPath.c_str());

            std::wstring szCommandLine = L"\"program.exe\" --option 1 \"-\" \"-\"";
            Assert::AreEqual(szCommandLine.c_str(), cl.szCommandLine.c_str());
        }
    };
}