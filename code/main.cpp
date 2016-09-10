/*--!>
This file is part of Recurse, a simple recursive file scanner written in C++.

Copyright 2015-2016 outshined (outshined@riseup.net)
    (PGP: 0x8A80C12396A4836F82A93FA79CA3D0F7E8FBCED6)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
--------------------------------------------------------------------------<!--*/
#include <nebula/foundation/exception.h>
#include <nebula/foundation/format.h>
#include <nebula/foundation/cstring.h>
#include <nebula/foundation/filesystem.h>
#include <nebula/foundation/scope_exit.h>
#include <nebula/foundation/regex.h>
#include <nebula/foundation/opts.h>
#include <nebula/foundation/qlog.h>

namespace fnd = nebula::foundation;
namespace fs = fnd::filesystem;
namespace fmt = fnd::fmt;
namespace io = fnd::io;
namespace regex = fnd::regex;
namespace math = fnd::math;

//------------------------------------------------------------------------------
struct runtime_error : public virtual fnd::runtime_error {};
struct logic_error : public virtual fnd::logic_error {};

//------------------------------------------------------------------------------
constexpr unsigned regular_f            = unsigned(1);
constexpr unsigned directory_f          = unsigned(1) << 1;
constexpr unsigned symlink_f            = unsigned(1) << 2;
constexpr unsigned block_f              = unsigned(1) << 3;
constexpr unsigned fifo_f               = unsigned(1) << 4;
constexpr unsigned socket_f             = unsigned(1) << 5;
constexpr unsigned character_device_f   = unsigned(1) << 6;
constexpr unsigned all_f =
    regular_f | symlink_f | directory_f
    | block_f | fifo_f | socket_f | character_device_f;

//------------------------------------------------------------------------------
const fnd::const_cstring program_author = "outshined (outshined@riseup.net)";
const fnd::const_cstring program_bugreport = PACKAGE_BUGREPORT;
const fnd::const_cstring program_url = PACKAGE_URL;
const fnd::const_cstring program_name = PACKAGE_NAME;
const fnd::const_cstring program_version = PACKAGE_VERSION;
const fnd::const_cstring program_description =
    "Recursive Filesystem Scanner";
const fnd::const_cstring program_license = 
    "Copyright 2015-2016 outshined (outshined@riseup.net)\n"
    "(PGP: 0x8A80C12396A4836F82A93FA79CA3D0F7E8FBCED6)\n\n"

    "This program is free software: you can redistribute it and/or modify\n"
    "it under the terms of the GNU Affero General Public License as\n"
    "published by the Free Software Foundation, either version 3 of the\n"
    "License, or (at your option) any later version.\n\n"

    "This program is distributed in the hope that it will be useful,\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    "GNU Affero General Public License for more details.\n\n"

    "You should have received a copy of the GNU Affero General Public License\n"
    "along with this program.  If not, see <http://www.gnu.org/licenses/>.";

//------------------------------------------------------------------------------
inline void print_version()
{
    fmt::fwrite(io::cout, program_name, ' ', program_version, fmt::endl);
}
//------------------------------------------------------------------------------
inline void print_license()
{
    fmt::fwrite(io::cout, program_license, fmt::endl);
}
//------------------------------------------------------------------------------
inline void print_help(const fnd::const_cstring argv0)
{
    fmt::fwrite(io::cout,
        program_description, fmt::endl,
        "Please send bugreports to ", program_bugreport, fmt::endl, fmt::endl,
        
"Usage: ", argv0, " [PATH | OPTION]...", fmt::endl, fmt::endl,
"PATH must be a valid path to a file (directory), like '/usr/bin' or '/bin/sh'.", fmt::endl,
"Providing no PATH is equivalent to providing the current working directory.", fmt::endl,
"An isolated single dash '-' will cause the next argument, wether or not it", fmt::endl,
"starts with either '-' or '--' to be understood as a PATH instead of an OPTION.", fmt::endl,
"An isolated double dash '--' works similarly but for all following arguments.", fmt::endl, fmt::endl,

"Below is a list of available OPTIONs. Every OPTION can be indicated with both", fmt::endl,
"a single dash '-' or a double dash '--'. So '-help' is equivalent to '--help'.", fmt::endl, fmt::endl,
                
"-h --help          Show this message.", fmt::endl,
"--license          Show the license.", fmt::endl,
"--version          Show the version.", fmt::endl,
"-v --verbose       Verbose error messages intended for debugging.", fmt::endl,

"-d --depth         Maximum search depth. [infinite]", fmt::endl,
"-t --type          The type of files to scan for. [f]", fmt::endl,
"                   Multiple options are allowed.", fmt::endl,
"                   For example \"sf\" indicates a search for regular and", fmt::endl,
"                   symlink files. Below is a list of available options.", fmt::endl,
"                       f ... regular files", fmt::endl,
"                       s ... symlink files", fmt::endl,
"                       d ... directory files", fmt::endl,
"                       b ... block files", fmt::endl,
"                       i ... fifo files", fmt::endl,
"                       k ... socket files", fmt::endl,
"                       c ... character device", fmt::endl,
"                       a ... all files", fmt::endl,
"-c --canonical     Make all paths absolute (canonical).", fmt::endl,
"-n --newline       Append a newline (\\n) after each output. If not set, then", fmt::endl,
"                   outputs are seperated by spaces.", fmt::endl,
"-r --regex         A regular expression to filter paths. [.*]", fmt::endl,
"-rt --regex-type   Set the regular expression type. [ecma]", fmt::endl,
"                   Available options are: egrep, ecma, posix, eposix.", fmt::endl,
"-rm --regex-match  Match the path exactly.", fmt::endl,
"                   By default the regular expression engine 'searches' the path", fmt::endl,
"                   and selects the path if only a fragment of it matches.", fmt::endl,
"-i --interpolate   Format the output using string interpolation.", fmt::endl,
"                       %%  ... print %", fmt::endl,
"                       %0% ... path", fmt::endl,
"                       %1% ... file type", fmt::endl,
"                       %2% ... file size", fmt::endl,
"                       %3% ... last access", fmt::endl,
"                       %4% ... last modification", fmt::endl,
"                       %5% ... last status change", fmt::endl,
"                       %6% ... permissions", fmt::endl,
"                       %7% ... depth", fmt::endl, fmt::endl,

"Example: ", argv0, " /bin . -c -n -i='File %0% is %2% bytes big.'", fmt::endl);
}

//------------------------------------------------------------------------------
fnd::intrusive_ptr<fnd::qlog::logger> gl;
//------------------------------------------------------------------------------
inline static auto init_log() noexcept
{
    namespace qlog = fnd::qlog;
    
    try
    {
        gl = new qlog::logger();
        gl->threshold(qlog::level::warning);
        gl->formatter(qlog::formatter::capture(
            [] (qlog::level lvl, fnd::string &s){
                io::msink<fnd::string> ss;
                fmt::fwrite(ss, 
                    "[ ", qlog::to_cstr(lvl), " ] ", s);
                s = fnd::move(ss.container());
            }));
        qlog::sink::init_console();
        gl->sink(qlog::sink::console());
        gl->error_handler(
            [&] (fnd::exception_ptr x) {
                try {
                    fmt::fwrite(io::cerr, 
                        "*** Log Error ***", fmt::endl,
                        fnd::diagnostic_information(fnd::current_exception()),
                        fmt::endl);
                } catch(...) {} // eat exception
            });
    }
    catch(...)
    {
        fmt::fwrite(io::cerr, 
            "*** Log Error ***", fmt::endl,
            fnd::diagnostic_information(fnd::current_exception()),
            fmt::endl);
        throw; // terminate
    }
}

//------------------------------------------------------------------------------
int main(int argc, char **argv)
{
    init_log();
    
    try {
        fnd::vector<fnd::const_cstring> opt_paths;
        unsigned opt_type_mask = regular_f;
        size_t opt_max_depth = math::maximum<size_t>();
        bool opt_canonical = false;
        bool opt_newline = false;
        fnd::const_cstring opt_interpolate;
        fnd::const_cstring opt_rx;
        regex::regex_option_type opt_rx_type = regex::ecma;
        bool opt_rx_search = true;
        int quit = -1;
        
        auto opts_ctx = fnd::opts::context(
            [&] (fnd::const_cstring id, fnd::const_cstring val,
                const size_t i)
            {
                gl->error("Unknown parameter: '-", id, "'.");
                quit = EXIT_FAILURE;
            },
            [&] (fnd::const_cstring val, const size_t i) {
                if(i > 0)
                    opt_paths.push_back(val);
                return true;
            },
            
            fnd::opts::argument(
                [&] (fnd::const_cstring id, fnd::const_cstring val, size_t i) {
                    if(!val.empty())
                        gl->warning("Value ignored. '-", id, "' is a flag.");
                    print_help(argv[0]);
                    quit = EXIT_SUCCESS;
                    return false;
                },
                "help", "h"),
            fnd::opts::argument(
                [&] (fnd::const_cstring id, fnd::const_cstring val, size_t i) {
                    if(!val.empty())
                        gl->warning("Value ignored. '-", id, "' is a flag.");
                    print_license();
                    quit = EXIT_SUCCESS;
                    return false;
                },
                "license"),
            fnd::opts::argument(
                [&] (fnd::const_cstring id, fnd::const_cstring val, size_t i) {
                    if(!val.empty())
                        gl->warning("Value ignored. '-", id, "' is a flag.");
                    print_version();
                    quit = EXIT_SUCCESS;
                    return false;
                },
                "version"),
                
            fnd::opts::argument(
                [&] (fnd::const_cstring id, fnd::const_cstring val, size_t i) {
                    if(!val.empty())
                        gl->warning("Value ignored. '-", id, "' is a flag.");
                    if(1 != i)
                        gl->warning("'-", id,
                            "' should be the first argument!");
                    gl->threshold(fnd::qlog::level::debug);
                    return true;
                },
                "verbose", "v"),
            
            fnd::opts::argument(
                [&] (fnd::const_cstring id, fnd::const_cstring val, size_t i) {
                    if(val.empty())
                    {
                        quit = EXIT_FAILURE;
                        gl->error("Missing value. '-", id, "=?'.");
                        return false;
                    }
                    
                    unsigned mask = 0;
                    for(const char c : val)
                    {
                        switch(c)
                        {
                        case 'f':
                            mask |= regular_f;
                            break;
                        case 's':
                            mask |= symlink_f;
                            break;
                        case 'd':
                            mask |= directory_f;
                            break;
                        case 'b':
                            mask |= block_f;
                            break;
                        case 'i':
                            mask |= fifo_f;
                            break;
                        case 'k':
                            mask |= socket_f;
                            break;
                        case 'c':
                            mask |= character_device_f;
                            break;
                        case 'a':
                            mask = all_f;
                        default:
                            quit = EXIT_FAILURE;
                            gl->error("Invalid value. '-", id, "=#ERROR'");
                            return false;
                        }
                    }
                    
                    opt_type_mask = mask;
                    
                    return true;
                },
                "type", "t"),
            fnd::opts::argument(
                [&] (fnd::const_cstring id, fnd::const_cstring val, size_t i) {
                    if(val.empty())
                    {
                        quit = EXIT_FAILURE;
                        gl->error("Missing value. '-", id, "=?'.");
                        return false;
                    }
                    auto r = fmt::to_integer<size_t>(
                        val, 10, fnd::nothrow_tag());
                    if(!r.valid())
                    {
                        quit = EXIT_FAILURE;
                        gl->error("Expected a positive number. ",
                            "--", id, "=#ERROR");
                        return false;
                    }
                    opt_max_depth = r.get();
                    return true;
                },
                "depth", "d"),
            fnd::opts::argument(
                [&] (fnd::const_cstring id, fnd::const_cstring val, size_t i) {
                    if(!val.empty())
                        gl->warning("Value ignored. '-", id, "' is a flag.");
                    opt_canonical = true;
                    return true;
                },
                "canonical", "c"),
            fnd::opts::argument(
                [&] (fnd::const_cstring id, fnd::const_cstring val, size_t i) {
                    if(!val.empty())
                        gl->warning("Value ignored. '-", id, "' is a flag.");
                    opt_newline = true;
                    return true;
                },
                "newline", "n"),
            fnd::opts::argument(
                [&] (fnd::const_cstring id, fnd::const_cstring val, size_t i) {
                    if(val.empty())
                    {
                        quit = EXIT_FAILURE;
                        gl->error("Missing value. '-", id, "=?'.");
                        return false;
                    }
                    opt_interpolate = val;
                    return true;
                },
                "interpolate", "i"),
            fnd::opts::argument(
                [&] (fnd::const_cstring id, fnd::const_cstring val, size_t i) {
                    if(val.empty())
                    {
                        quit = EXIT_FAILURE;
                        gl->error("Missing value. '-", id, "=?'.");
                        return false;
                    }
                    opt_rx = val;
                    return true;
                },
                "regex", "r"),
            fnd::opts::argument(
                [&] (fnd::const_cstring id, fnd::const_cstring val, size_t i) {
                    if(val.empty())
                    {
                        quit = EXIT_FAILURE;
                        gl->error("Missing value. '-", id, "=?'.");
                        return false;
                    }
                    
                    if(val == "grep")
                        opt_rx_type = regex::grep;
                    else if(val == "egrep")
                        opt_rx_type = regex::egrep;
                    else if(val == "ecma")
                        opt_rx_type = regex::ecma;
                    else if(val == "posix")
                        opt_rx_type = regex::basic_posix;
                    else if(val == "eposix")
                        opt_rx_type = regex::extended_posix;
                    else
                    {
                        quit = EXIT_FAILURE;
                        gl->error("Invalid value. '-", id, "=#ERROR'");
                        return false;
                    }
                    
                    return true;
                },
                "regex-type", "rt"),
            fnd::opts::argument(
                [&] (fnd::const_cstring id, fnd::const_cstring val, size_t i) {
                    if(!val.empty())
                        gl->warning("Value ignored. '-", id, "' is a flag.");
                    opt_rx_search = false;
                    return true;
                },
                "regex-match", "rm"));
        
        fnd::opts::parse_command_line(opts_ctx, argc, argv);
        
        if(quit != -1)
            return quit;
        
        regex::default_regex *rx = nullptr;
        n_scope_exit(&) {
            if(rx)
                delete rx;
        };
        if(!opt_rx.empty() && opt_rx != ".*")
        {
            try {
                rx = new regex::default_regex(
                    opt_rx, regex::optimize | opt_rx_type);
            } catch(const regex::regex_error &e) {
                gl->error("Compiling a regular expression failed.");
                gl->debug(fnd::diagnostic_information(e));
                return EXIT_FAILURE;
            }
        }
        
        auto f = [&](fs::path p, size_t level) -> bool
        {   
            const fs::file_status stat = fs::status(p);
 
            switch(fs::type(stat))
            {
            case fs::file_type::regular:
                if((opt_type_mask & regular_f) == 0u)
                    return true;
                break;
            case fs::file_type::symlink:
                if((opt_type_mask & symlink_f) == 0u)
                    return true;
                break;
            case fs::file_type::directory:
                if((opt_type_mask & directory_f) == 0u)
                    return true;
                break;
            case fs::file_type::block:
                if((opt_type_mask & block_f) == 0u)
                    return true;
                break;
            case fs::file_type::fifo:
                if((opt_type_mask & fifo_f) == 0u)
                    return true;
                break;
            case fs::file_type::socket:
                if((opt_type_mask & socket_f) == 0u)
                    return true;
                break;
            case fs::file_type::character_device:
                if((opt_type_mask & character_device_f) == 0u)
                    return true;
                break;
            default:
                n_throw(logic_error);
            }
            
            if(rx)
            {
                if(opt_rx_search) {
                    if(!regex::search(p.str(), *rx))
                        return true;
                } else {
                    if(!regex::match(p.str(), *rx))
                        return true;
                }
            }
            
            if(opt_interpolate.empty())
            {
                fmt::fwrite(io::cout, p);
            }
            else
            {
                try {
                    fmt::interp(
                        io::cout,
                        opt_interpolate,
                        p,
                        fs::to_cstr(fs::type(stat)),
                        fs::size(stat),
                        fs::last_access(stat),
                        fs::last_modification(stat),
                        fs::last_status_change(stat),
                        fs::pretty_permissions(stat),
                        level);
                } catch(...) {
                    n_throw(runtime_error)
                    << fnd::ei_msg_c("String interpolation failed.")
                    << fnd::ei_exc(fnd::current_exception())
                    << fs::ei_path(p);
                }
            }
            
            if(opt_newline)
                io::put(io::cout, '\n');
            else
                io::put(io::cout, ' ');
            
            return true;
        };
        
        if(opt_paths.empty())
            opt_paths.emplace_back(".");
        
        for(fnd::const_cstring x : opt_paths)
        {
            const fs::path path = opt_canonical
                ? fs::canonical(x)
                : x;
            fs::recursive_scan(path, opt_max_depth, f);
        }
    }
    catch(const fnd::exception &e)
    {
        const fs::ei_path *p = fnd::get_error_info<fs::ei_path>(e);
        
        gl->fatal("Internal Error");
        
        if(p)
            gl->fatal("Path: ", p->value());
        
        gl->debug(fnd::diagnostic_information(e));
        
        return EXIT_FAILURE;
    }
    catch(...)
    {
        gl->fatal("Internal Error");
        gl->debug(fnd::diagnostic_information(fnd::current_exception()));
        
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
