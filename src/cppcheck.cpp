/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2009 Daniel Marjamäki, Reijo Tomperi, Nicolas Le Cam,
 * Leandro Penz, Kimmo Varis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/
 */
#include "cppcheck.h"

#include "preprocessor.h" // preprocessor.
#include "tokenize.h"   // <- Tokenizer

#include "checkmemoryleak.h"
#include "checkbufferoverrun.h"
#include "checkdangerousfunctions.h"
#include "checkclass.h"
#include "checkheaders.h"
#include "checkother.h"
#include "checkfunctionusage.h"
#include "filelister.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstring>
#include <fstream>
#include <map>

//---------------------------------------------------------------------------

CppCheck::CppCheck(ErrorLogger &errorLogger)
{
    _errorLogger = &errorLogger;
}

CppCheck::~CppCheck()
{

}

void CppCheck::settings(const Settings &settings)
{
    _settings = settings;
}

void CppCheck::addFile(const std::string &path)
{
    _filenames.push_back(path);
}

void CppCheck::addFile(const std::string &path, const std::string &content)
{
    _filenames.push_back(path);
    _fileContents[ path ] = content;
}

std::string CppCheck::parseFromArgs(int argc, const char* const argv[])
{
    std::vector<std::string> pathnames;
    bool showHelp = false;
    for (int i = 1; i < argc; i++)
    {
        // Flag used for various purposes during debugging
        if (strcmp(argv[i], "--debug") == 0)
            _settings._debug = true;

        // Show all messages
        else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0)
            _settings._showAll = true;

        // Only print something when there are errors
        else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0)
            _settings._errorsOnly = true;

        // Checking coding style
        else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--style") == 0)
            _settings._checkCodingStyle = true;

        // Verbose error messages (configuration info)
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
            _settings._verbose = true;

        // Force checking of files that have "too many" configurations
        else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0)
            _settings._force = true;

        // Write results in results.xml
        else if (strcmp(argv[i], "--xml") == 0)
            _settings._xml = true;

        // Check if there are unused functions
        else if (strcmp(argv[i], "--unused-functions") == 0)
            _settings._unusedFunctions = true;

        // Print help
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            pathnames.clear();
            _filenames.clear();
            showHelp = true;
            break;
        }

        // Include paths
        else if (strcmp(argv[i], "-I") == 0 || strncmp(argv[i], "-I", 2) == 0)
        {
            std::string path;

            // "-I path/"
            if (strcmp(argv[i], "-I") == 0)
            {
                ++i;
                if (i >= argc)
                    return "cppcheck: argument to '-I' is missing\n";

                path = argv[i];
            }

            // "-Ipath/"
            else
            {
                path = argv[i];
                path = path.substr(2);
            }

            // If path doesn't end with / or \, add it
            if (path[path.length()-1] != '/' && path[path.length()-1] != '\\')
                path += '/';

            _includePaths.push_back(path);
        }

        else if (strncmp(argv[i], "-", 1) == 0 || strncmp(argv[i], "--", 2) == 0)
        {
            return "cppcheck: error: unrecognized command line option \"" + std::string(argv[i]) + "\"\n";
        }

        else
            pathnames.push_back(argv[i]);
    }

    if (pathnames.size() > 0)
    {
        // Execute RecursiveAddFiles() to each given file parameter
        std::vector<std::string>::const_iterator iter;
        for (iter = pathnames.begin(); iter != pathnames.end(); iter++)
            FileLister::RecursiveAddFiles(_filenames, iter->c_str(), true);
    }


    if (argc <= 1 || showHelp)
    {
        std::ostringstream oss;
        oss <<   "Cppcheck 1.28\n"
        "\n"
        "A tool for static C/C++ code analysis\n"
        "\n"
        "Syntax:\n"
        "    cppcheck [--all] [--force] [--help] [-Idir] [--quiet] [--style]\n"
        "             [--unused-functions] [--verbose] [--xml]\n"
        "             [file or path1] [file or path]\n"
        "\n"
        "If path is given instead of filename, *.cpp, *.cxx, *.cc, *.c++ and *.c files\n"
        "are checked recursively from given directory.\n\n"
        "Options:\n"
        "    -a, --all            Make the checking more sensitive. More bugs are\n"
        "                         detected, but there are also more false positives\n"
        "    -f, --force          Force checking on files that have \"too many\"\n"
        "                         configurations\n"
        "    -h, --help           Print this help\n"
        "    -I <dir>             Give include path. Give several -I parameters to give\n"
        "                         several paths. First given path is checked first. If\n"
        "                         paths are relative to source files, this is not needed\n"
        "    -q, --quiet          Only print error messages\n"
        "    -s, --style          Check coding style\n"
        "    --unused-functions   Check if there are unused functions\n"
        "    -v, --verbose        More detailed error reports\n"
        "    --xml                Write results in results.xml\n"
        "\n"
        "Example usage:\n"
        "  # Recursively check the current folder. Print the progress on the screen and\n"
        "    write errors in a file:\n"
        "    cppcheck . 2> err.txt\n"
        "  # Recursively check ../myproject/ and print only most fatal errors:\n"
        "    cppcheck --quiet ../myproject/\n"
        "  # Check only files one.cpp and two.cpp and give all information there is:\n"
        "    cppcheck -v -a -s one.cpp two.cpp\n"
        "  # Check f.cpp and search include files from inc1/ and inc2/:\n"
        "    cppcheck -I inc1/ -I inc2/ f.cpp\n";
        return oss.str();
    }
    else if (_filenames.empty())
    {
        return "cppcheck: No C or C++ source files found.\n";
    }


    return "";
}

unsigned int CppCheck::check()
{
    _checkFunctionUsage.setErrorLogger(this);
    std::sort(_filenames.begin(), _filenames.end());
    for (unsigned int c = 0; c < _filenames.size(); c++)
    {
        _errout.str("");
        std::string fname = _filenames[c];

        if (_settings._errorsOnly == false)
            _errorLogger->reportOut(std::string("Checking ") + fname + std::string("..."));

        Preprocessor preprocessor;
        std::list<std::string> configurations;
        std::string filedata = "";
        if (_fileContents.size() > 0 && _fileContents.find(_filenames[c]) != _fileContents.end())
        {
            // File content was given as a string
            std::istringstream iss(_fileContents[ _filenames[c] ]);
            preprocessor.preprocess(iss, filedata, configurations, fname, _includePaths);
        }
        else
        {
            // Only file name was given, read the content from file
            std::ifstream fin(fname.c_str());
            preprocessor.preprocess(fin, filedata, configurations, fname, _includePaths);
        }

        int checkCount = 0;
        for (std::list<std::string>::const_iterator it = configurations.begin(); it != configurations.end(); ++it)
        {
            // Check only 12 first configurations, after that bail out, unless --force
            // was used.
            if (!_settings._force && checkCount > 11)
            {
                if (_settings._errorsOnly == false)
                    _errorLogger->reportOut(std::string("Bailing out from checking ") + fname + ": Too many configurations. Recheck this file with --force if you want to check them all.");

                break;
            }

            cfg = *it;
            std::string codeWithoutCfg = Preprocessor::getcode(filedata, *it);

            // If only errors are printed, print filename after the check
            if (_settings._errorsOnly == false && it != configurations.begin())
                _errorLogger->reportOut(std::string("Checking ") + fname + ": " + cfg + std::string("..."));

            checkFile(codeWithoutCfg, _filenames[c].c_str());
            ++checkCount;
        }

        if (_settings._errorsOnly == false && _errout.str().empty())
        {
            std::ostringstream oss;
            oss << "No errors found ("
            << (c + 1) << "/" << _filenames.size()
            << " files checked " <<
            static_cast<int>(static_cast<double>((c + 1)) / _filenames.size()*100)
            << "% done)";
            _errorLogger->reportOut(oss.str());
        }
    }

    // This generates false positives - especially for libraries
    _settings._verbose = false;
    if (_settings._unusedFunctions)
    {
        _errout.str("");
        if (_settings._errorsOnly == false)
            _errorLogger->reportOut("Checking usage of global functions (this may take several minutes)..");

        _checkFunctionUsage.check();
    }

    if (_settings._xml)
    {
        std::ofstream fxml("result.xml");
        fxml << "<?xml version=\"1.0\"?>\n";
        fxml << "<results>\n";
        for (std::list<std::string>::const_iterator it = _xmllist.begin(); it != _xmllist.end(); ++it)
            fxml << "  " << *it << "\n";
        fxml << "</results>";
    }

    unsigned int result = static_cast<unsigned int>(_errorList.size());
    _errorList.clear();
    return result;
}


//---------------------------------------------------------------------------
// CppCheck - A function that checks a specified file
//---------------------------------------------------------------------------

void CppCheck::checkFile(const std::string &code, const char FileName[])
{
    Tokenizer _tokenizer;

    // Tokenize the file
    {
        std::istringstream istr(code);
        _tokenizer.tokenize(istr, FileName);
    }

    // Set variable id
    _tokenizer.setVarId();

    _tokenizer.fillFunctionList();

    // Check that the memsets are valid.
    // The 'memset' function can do dangerous things if used wrong.
    // Important: The checking doesn't work on simplified tokens list.
    CheckClass checkClass(&_tokenizer, _settings, this);
    if (ErrorLogger::memsetClass())
        checkClass.noMemset();


    // Coding style checks that must be run before the simplifyTokenList
    CheckOther checkOther(&_tokenizer, _settings, this);

    // Check for unsigned divisions where one operand is signed
    if (ErrorLogger::udivWarning(_settings) || ErrorLogger::udivError())
        checkOther.CheckUnsignedDivision();

    // Give warning when using char variable as array index
    if (ErrorLogger::charArrayIndex(_settings) || ErrorLogger::charBitOp(_settings))
        checkOther.CheckCharVariable();

    _tokenizer.simplifyTokenList();

    if (_settings._unusedFunctions)
        _checkFunctionUsage.parseTokens(_tokenizer);

    // Class for detecting buffer overruns and related problems
    CheckBufferOverrunClass checkBufferOverrun(&_tokenizer, _settings, this);

    // Class for checking functions that should not be used
    CheckDangerousFunctionsClass checkDangerousFunctions(&_tokenizer, _settings, this);

    // Memory leak
    CheckMemoryLeakClass checkMemoryLeak(&_tokenizer, _settings, this);
    if (ErrorLogger::memleak() || ErrorLogger::mismatchAllocDealloc())
        checkMemoryLeak.CheckMemoryLeak();

    // Check that all class constructors are ok.
    if (ErrorLogger::noConstructor(_settings) || ErrorLogger::uninitVar())
        checkClass.constructors();

    // Check that all base classes have virtual destructors
    if (ErrorLogger::virtualDestructor())
        checkClass.virtualDestructor();

    // Array index out of bounds / Buffer overruns..
    if (ErrorLogger::arrayIndexOutOfBounds(_settings) || ErrorLogger::bufferOverrun(_settings))
        checkBufferOverrun.bufferOverrun();

    // Warning upon c-style pointer casts
    if (ErrorLogger::cstyleCast(_settings))
    {
        const char *ext = strrchr(FileName, '.');
        if (ext && strcmp(ext, ".cpp") == 0)
            checkOther.WarningOldStylePointerCast();
    }

    // if (a) delete a;
    if (ErrorLogger::redundantIfDelete0(_settings))
        checkOther.WarningRedundantCode();

    // strtol and strtoul usage
    if (ErrorLogger::dangerousUsageStrtol() ||
        ErrorLogger::sprintfOverlappingData())
        checkOther.InvalidFunctionUsage();

    // Check that all private functions are called.
    if (ErrorLogger::unusedPrivateFunction(_settings))
        checkClass.privateFunctions();

    // 'operator=' should return something..
    if (ErrorLogger::operatorEq(_settings))
        checkClass.operatorEq();

    // if (condition);
    if (ErrorLogger::ifNoAction(_settings) || ErrorLogger::conditionAlwaysTrueFalse(_settings))
        checkOther.WarningIf();

    // Unused struct members..
    if (ErrorLogger::unusedStructMember(_settings))
        checkOther.CheckStructMemberUsage();

    // Check if a constant function parameter is passed by value
    if (ErrorLogger::passedByValue(_settings))
        checkOther.CheckConstantFunctionParameter();

    // Variable scope (check if the scope could be limited)
    if (ErrorLogger::variableScope())
        checkOther.CheckVariableScope();

    // Check for various types of incomplete statements that could for example
    // mean that an ';' has been added by accident
    if (ErrorLogger::constStatement(_settings))
        checkOther.CheckIncompleteStatement();

    // Unusual pointer arithmetic
    if (ErrorLogger::strPlusChar())
        checkOther.strPlusChar();
}
//---------------------------------------------------------------------------

void CppCheck::reportErr(const std::string &errmsg)
{
    // Alert only about unique errors
    if (std::find(_errorList.begin(), _errorList.end(), errmsg) != _errorList.end())
        return;

    _errorList.push_back(errmsg);
    std::string errmsg2(errmsg);
    if (_settings._verbose)
    {
        errmsg2 += "\n    Defines=\'" + cfg + "\'\n";
    }


    _errorLogger->reportErr(errmsg2);

    _errout << errmsg2 << std::endl;
}

void CppCheck::reportOut(const std::string & /*outmsg*/)
{
    // This is currently never called. It is here just to comply with
    // the interface.
}

void CppCheck::reportXml(const std::string &file, const std::string &line, const std::string &id, const std::string &severity, const std::string &msg)
{
    std::ostringstream xml;
    xml << "<error";
    xml << " file=\"" << file << "\"";
    xml << " line=\"" << line << "\"";
    xml << " id=\"" << id << "\"";
    xml << " severity=\"" << severity << "\"";
    xml << " msg=\"" << msg << "\"";
    xml << "/>";

    _xmllist.push_back(xml.str());
}
