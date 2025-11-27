
#include <map>
#include <string>
#include <tuple>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <random>
#include <optional>
#include <fstream>
#include <thread>
#include <cstring>
#include <iterator>
#include <climits>

#define LIBISOFS_WITHOUT_LIBBURN yes
#include <libisofs.h>

#include "disc_image/disc_image.h"

#include "IP.BIN.include"
#include "default.mr.include"

#include "scramble.h"
#include "elf_parser.hpp"

#ifdef _WIN32
#define PATH_TO_CSTR(p) (p).string().c_str()
#else
#define PATH_TO_CSTR(p) (p).c_str()
#endif

static std::map<std::string, std::vector<std::string>> OPTS;

static std::map<std::string, int> SORT_WEIGHTS;

enum ArgType {
    ARG_TYPE_NAMED_OPTIONAL,
    ARG_TYPE_NAMED_REQUIRED,
    ARG_TYPE_FLAG_OPTIONAL,
    ARG_TYPE_FLAG_REQUIRED,
};

const std::vector<std::tuple<std::string, std::string, std::string, ArgType>> COMMANDS = {
    {"-a", "--author", "author of the disc/game", ARG_TYPE_NAMED_OPTIONAL},
    {"-b", "--unscrambled-binary", "executable file to use as 1ST_READ.BIN, in unscrambled binary format", ARG_TYPE_NAMED_OPTIONAL},
    {"-B", "--scrambled-binary", "executable file to use as 1ST_READ.BIN, in scrambled binary format", ARG_TYPE_NAMED_OPTIONAL},
    {"-c", "--cdda", ".wav file to use as an audio track. Specify multiple times to create multiple tracks", ARG_TYPE_NAMED_OPTIONAL},
    {"-d", "--directory", "directory to include (recursively) in the data track. Repeat for multiple directories", ARG_TYPE_NAMED_OPTIONAL},
    {"-D", "--directory-contents", "directory whose contents should be included (recursively) in the data track. Repeat for multiple directories", ARG_TYPE_NAMED_OPTIONAL},
    {"-e", "--elf", "executable file to use as 1ST_READ.BIN", ARG_TYPE_NAMED_OPTIONAL},
    {"-f", "--file", "file to include in the data track. Repeat for multiple files", ARG_TYPE_NAMED_OPTIONAL},
    {"-h", "--help", "this help screen", ARG_TYPE_FLAG_OPTIONAL},
    {"-i", "--image", "path to a suitable MR format image for the license screen", ARG_TYPE_NAMED_OPTIONAL},
    {"-m", "--no-mr", "disable the default MR boot image", ARG_TYPE_FLAG_OPTIONAL},
    {"-I", "--dump-iso", "if specified, the data track will be written to a .iso alongside the .cdi", ARG_TYPE_FLAG_OPTIONAL},
    {"-o", "--output", "output filename", ARG_TYPE_NAMED_REQUIRED},
    {"-n", "--name", "name of the game (must be fewer than 128 characters)", ARG_TYPE_NAMED_OPTIONAL},
    {"-N", "--no-padding", "specify to disable padding of the data track", ARG_TYPE_FLAG_OPTIONAL},
    {"-p", "--ipbin", "ip.bin file to use instead of the default one", ARG_TYPE_NAMED_OPTIONAL},
    {"-q", "--quiet", "disable logging. equivalent to 'v 0'", ARG_TYPE_FLAG_OPTIONAL},
    {"-r", "--release", "release date in YYYYMMDD format", ARG_TYPE_NAMED_OPTIONAL},
    {"-s", "--serial", "disk serial number", ARG_TYPE_NAMED_OPTIONAL},
    {"-S", "--sort-file", "path to sort file", ARG_TYPE_NAMED_OPTIONAL},
    {"-v", "--verbosity", "a number between 0 and 3, 0 == no output", ARG_TYPE_NAMED_OPTIONAL}
};

static int verbosity() {
    if(OPTS.count("quiet")) {
        return 0;
    }

    auto level = (OPTS.count("verbosity")) ? OPTS["verbosity"][0] : "1";
    return std::stoi(level);
}


const char* HELP_TEXT_PREAMBLE = R"(
Usage: mkdcdisc [OPTION]... -e [EXECUTABLE] -o [OUTPUT]
Generate a DiscJuggler .cdi file for use with the SEGA Dreamcast.

)";

static void print_help() {
    std::cout << HELP_TEXT_PREAMBLE;

    size_t max_width = 0;
    for(auto cmd: COMMANDS) {
        max_width = std::max(std::get<1>(cmd).size(), max_width);
    }

    for(auto cmd: COMMANDS) {
        auto a = std::get<0>(cmd);
        auto b = std::get<1>(cmd);
        auto c = std::get<2>(cmd);

        std::cout << "  " << a << ", " << std::left << std::setw(max_width + 4) << std::setfill(' ') << b  << c << std::endl;
    }

    std::cout << std::endl;
}

static std::string normalize_command(const std::string& arg) {
    for(auto& t: COMMANDS) {
        if(std::get<0>(t) == arg || std::get<1>(t) == arg) {
            return std::get<1>(t);
        }
    }

    return "";
}

/* FIXME: Take required arguments without -- prefixes */
std::map<std::string, std::vector<std::string>> parse_options(int argc, char* argv[], bool* ok) {
    std::map<std::string, std::vector<std::string>> opts;

    assert(ok);

    std::string current_arg;
    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(current_arg.empty()) {
            /* Awaiting command */
            auto l = arg.length();
            if(l < 2 || arg[0] != '-') {
                std::cerr << "Invalid argument: " << arg << std::endl;
                *ok = false;
            }

            auto normalized = normalize_command(arg);
            if(normalized.empty()) {
                std::cerr << "Invalid argument: " << arg << std::endl;
                *ok = false;
                current_arg.clear();
                continue;
            }

            current_arg = normalized.substr(normalized.find_first_not_of("-"), std::string::npos);

            /* Check is valid */
            bool found = false;
            bool is_flag = false;
            for(auto& cmd: COMMANDS) {
                auto name = std::get<1>(cmd);
                if(name == normalized) {
                    found = true;
                    is_flag = std::get<3>(cmd) == ARG_TYPE_FLAG_OPTIONAL || std::get<3>(cmd) == ARG_TYPE_FLAG_REQUIRED;
                    break;
                }
            }

            if(!found) {
                std::cerr << "Invalid argument: " << arg << std::endl;
                exit(10); /* FIXME: Return optional<> and handle in main */
            }

            /* Don't expect a value */
            if(is_flag) {
                auto& values = opts[current_arg];
                values.push_back("true");
                current_arg.clear();
            }

        } else {
            /* Awaiting value */
            auto& values = opts[current_arg];
            values.push_back(arg);
            current_arg.clear();
        }
    }

    *ok = true;
    return opts;
}

static std::optional<std::filesystem::path> generate_temporary_directory() {
    auto temp_dir = std::filesystem::temp_directory_path();
    std::random_device dev;
    std::mt19937 prng(dev());
    std::uniform_int_distribution<uint64_t> rand(0);
    std::filesystem::path path;

    std::stringstream ss;
    ss << std::hex << rand(prng);
    path = temp_dir / ss.str();
    if(std::filesystem::create_directory(path)) {
        if(verbosity() > 2) {
            std::cout << "Created temporary directory at: " << path << std::endl;
        }
        return path;
    } else {
        return std::optional<std::filesystem::path>();
    }
}

void destroy_temp_directory(const std::filesystem::path& path) {
    /* Resolve and normalize paths to handle symbolic links */
    auto temp_dir_path = std::filesystem::canonical(std::filesystem::temp_directory_path());
    auto provided_path_parent = std::filesystem::canonical(path.parent_path());

    if(verbosity() > 2) {
        std::cout << "Checking paths:" << std::endl;
        std::cout << "  System temp dir: " << temp_dir_path << std::endl;
        std::cout << "  Provided dir's parent: " << provided_path_parent << std::endl;
    }

    /* Ensure the provided path is within the system's temp directory */
    if(provided_path_parent == temp_dir_path) {
        if(verbosity() > 2) {
            std::cout << "Removing temporary directory at: " << path << std::endl;
        }
        std::filesystem::remove_all(path);
    } else {
        std::cerr<< "Failed to remove temporary directory at : " << path << std::endl;
    }
}

bool load_elf_file(const std::filesystem::path& elf_name, std::vector<char>& bin_data) {
    if(!std::filesystem::exists(elf_name)) {
        std::cerr << "ELF file does not exist: " << elf_name << std::endl;
        return false;
    }

    if(verbosity() > 1) {
        std::cout << "Loading " << elf_name << std::endl;
    }

    auto elf_parser_ret = elfparser::Parser::Load(elf_name);
    if (!elf_parser_ret.has_value()) {
        std::cerr << "Failed to load ELF " << elf_name << std::endl;
        return false;
    }
    auto elf_parser = elf_parser_ret.value();
    
    // Create BIN from  ELF
    if ( !elf_parser->fill_bin(bin_data)) {
        std::cerr << "Failed to create BIN" << std::endl;
        return false;
    }

    return true;
}

bool load_bin_file(const std::filesystem::path& bin_path, std::vector<char>& bin_data) {
    if(!std::filesystem::exists(bin_path)) {
        std::cerr << "BIN file does not exist: " << bin_path << std::endl;
        return false;
    }

    if(verbosity() > 1) {
        std::cout << "Loading " << bin_path << std::endl;
    }

    std::ifstream file;
    std::streamsize file_size = 0;

    file.open(bin_path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Unable to open BIN file: " << bin_path << std::endl;
        return false;
    }

    file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    bin_data.resize(file_size);
    if (!file.read(&bin_data[0], file_size)) {
        std::cerr << "Unable to read BIN file: " << bin_path << std::endl;
        file.close();
        return false;
    }
    file.close();

    return true;
}

static bool gather_files(const std::filesystem::path& out) {
    if(verbosity() > 1) {
        std::cout << "Gathering files from specified directories" << std::endl;
    }

    std::vector<char> bin_data;
    std::vector<char> scrambled_data;
    bool needs_scrambling = true;

    if(OPTS.count("elf")) {
        load_elf_file(OPTS["elf"][0], bin_data);
    } else if (OPTS.count("unscrambled-binary")) {
        load_bin_file(OPTS["unscrambled-binary"][0], bin_data);
    } else {
        load_bin_file(OPTS["scrambled-binary"][0], bin_data);
        needs_scrambling = false;
    }

    if (!bin_data.size()) {
        std::cerr << "No bin data available, aborting..." << std::endl;
        return false;
    }

    if(verbosity() > 2) {
        std::cout << "Bin size: " << bin_data.size() << std::endl;
    }

    // Scramble BIN
    if (needs_scrambling) {
        scrambled_data = scramble(bin_data);
        if(verbosity() > 2) {
            std::cout << "Bin Scrambled!" << std::endl;
        }
    } else {
        scrambled_data.resize(bin_data.size());
        memcpy(&scrambled_data[0], &bin_data[0], bin_data.size());
    }

    std::filesystem::path scrambled_path = out / "1ST_READ.BIN";

    std::ofstream output_stream;
    output_stream.open(scrambled_path, std::ios::binary);
    if (!output_stream.is_open())
    {
        std::cerr << "Failed to open " << scrambled_path << std::endl;
        return false;
    }
    output_stream.write(&scrambled_data[0], scrambled_data.size());
    output_stream.close();

    if(verbosity() > 2) {
        std::cout << "Saved to " << scrambled_path << std::endl;
    }
    
    if(OPTS.count("directory")) {
        for(std::filesystem::path dir: OPTS["directory"]) {
            if(std::filesystem::exists(dir)) {
                std::filesystem::copy(dir, out / dir.stem(), std::filesystem::copy_options::recursive);
            } else {
                std::cerr << "No such directory: " << dir.string() << std::endl;
                return false;
            }
        }
    }

    if(OPTS.count("directory-contents")) {
        for(std::filesystem::path dir: OPTS["directory-contents"]) {
            if(std::filesystem::exists(dir)) {
                for (const auto & entry : std::filesystem::directory_iterator(dir)) {
                    if (entry.path().filename() != "1ST_READ.BIN") {
                        if (verbosity() > 2) {
                            std::cout << "Adding file: " << entry.path().filename() << std::endl;
                        }
                        std::filesystem::copy(entry, out / entry.path().filename(), std::filesystem::copy_options::recursive);
                    } else {
                        if (verbosity() > 2) {
                            std::cout << "Skipping 1ST_READ.BIN as we are generating a new one" << std::endl;
                        }
                    }
                }
            } else {
                std::cerr << "No such directory: " << dir.string() << std::endl;
                return false;
            }
        }
    }

    if(OPTS.count("file")) {
        for(std::filesystem::path file: OPTS["file"]) {
            if(std::filesystem::exists(file)) {
                if(verbosity() > 2) {
                    std::cout << file << " -> " << out / file.filename() << std::endl;
                }

                std::filesystem::copy(file, out / file.filename());
            } else {
                std::cerr << "No such file: " << file.string() << std::endl;
                return false;
            }
        }
    }

    return true;
}

struct IPBin {
    struct {
        char hardware_id[16];
        char maker_id[16];
        char device_info[16];
        char area_symbols[8];
        char peripherals[8];
        char product_number[10];
        char product_version[6];
        char release_date[16];
        char boot_filename[16];
        char company_name[16];
        char game_name[128];
    } meta;

    char toc[512] = {0};
    char license_screen[13312];
    char area_protection[256];
    char bootstrap1[10240];
    char bootstrap2[8192];
};

static std::string generate_product_number(const std::vector<uint8_t>& data) {
    const std::string prefix = "IND-";
    const std::string raw(data.begin(), data.end());

    auto hash = std::hash<std::string>()(raw);
    return prefix + std::to_string(hash).substr(0, 6);
}

bool load_ip_bin(const std::filesystem::path& ipbin_path, IPBin* out) {
    const std::streamsize MAX_IPBIN_SIZE = 32768;

    if(!std::filesystem::exists(ipbin_path)) {
        std::cerr << "IP.BIN file does not exist: " << ipbin_path << std::endl;
        return false;
    }

    std::ifstream file;
    std::streamsize file_size = 0;

    file.open(ipbin_path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Unable to open IP.BIN file: " << ipbin_path << std::endl;
        return false;
    }

    file_size = file.tellg();
    if (file_size > MAX_IPBIN_SIZE) {
        std::cerr << "IP.BIN file size exceeds the maximum allowed size of " 
            << MAX_IPBIN_SIZE << " bytes. File size is " << file_size << " bytes." 
            << std::endl;
        file.close();
        return false;
    }

    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(out), file_size)) {
        std::cerr << "Unable to read IP.BIN file: " << ipbin_path << std::endl;
        file.close();
        return false;
    }
    file.close();

    return true;
}

bool generate_ip_bin(const std::filesystem::path& output_dir, const std::filesystem::path& bin_path, IPBin* out) {
    std::ifstream bin(bin_path.string());
    bin.seekg(0, std::ios::end);
    auto size = bin.tellg();
    std::vector<uint8_t> bin_data;
    bin_data.resize(size);
    bin.seekg(0, std::ios::beg);
    bin.read((char*) &bin_data[0], size);
    bin.close();

    IPBin ip_bin;
    uint8_t* mr_image = ((uint8_t*) &ip_bin) + 0x3820;

    std::copy(IP_BIN, IP_BIN + sizeof(IP_BIN), (char*) &ip_bin);

    if(!OPTS.count("no-mr")) {
        if(OPTS.count("image")) {
            std::string image_path = OPTS["image"][0];
            const std::streamsize MAX_IMAGE_SIZE = 8192;

            bool valid_image = true;

            if(!std::filesystem::exists(image_path)) {
                std::cerr << "MR file does not exist: " << image_path << std::endl;
                valid_image = false;
            }

            std::ifstream file;
            std::streamsize file_size = 0;

            if(valid_image) {
                file.open(image_path, std::ios::binary | std::ios::ate);
                if (!file) {
                    std::cerr << "Unable to open MR file: " << image_path << std::endl;
                    valid_image = false;
                }
            }

            if(valid_image) {
                file_size = file.tellg();
                if (file_size > MAX_IMAGE_SIZE) {
                    std::cerr << "MR file size exceeds the maximum allowed size of " 
                            << MAX_IMAGE_SIZE << " bytes. File size is " << file_size << " bytes." 
                            << std::endl;
                    valid_image = false;
                }
            }

            if(valid_image) {
                file.seekg(0, std::ios::beg);
                if (!file.read(reinterpret_cast<char*>(mr_image), file_size)) {
                    std::cerr << "Unable to read MR file: " << image_path << std::endl;
                    valid_image = false;
                }
                file.close();
            }

            if(!valid_image) {
                std::cout << "Using default MR image due to errors." << std::endl;
                std::copy(default_mr, default_mr + default_mr_len, mr_image);
            } 
        } else {
            std::copy(default_mr, default_mr + default_mr_len, mr_image);
        }
    }

    auto pad = [](std::string s, std::size_t x) -> std::string {
        while(s.size() < x) {
            s += " ";
        }
        return s;
    };

    std::string name = OPTS.count("name") ? OPTS["name"][0] : "Untitled Game";
    name = pad(name, 128);
    std::strncpy(ip_bin.meta.game_name, name.c_str(), 128);

    std::string company = OPTS.count("author") ? OPTS["author"][0] : "Unknown Author";
    company = pad(company, 16);
    std::strncpy(ip_bin.meta.company_name, company.c_str(), 16);

    char generated_date[9];
    std::time_t t = std::time(NULL);
    std::strftime(generated_date, sizeof(generated_date), "%Y%m%d", std::localtime(&t));

    /* FIXME: Check format of the passed release date */
    std::string release = OPTS.count("release") ? OPTS["release"][0] : generated_date;
    release = pad(release, 16);
    std::strncpy(ip_bin.meta.release_date, release.c_str(), 16);

    std::string product_number = OPTS.count("serial") ? OPTS["serial"][0] : generate_product_number(bin_data);
    product_number = pad(product_number, 10);
    std::strncpy(ip_bin.meta.product_number, product_number.c_str(), 10);

    *out = ip_bin;
    return true;
}

std::optional<std::vector<uint8_t>> wav_to_cdda(const std::string& filename) {
    std::vector<uint8_t> data;

    std::ifstream file(filename, std::ios::binary);
    if(!file.good()) {
        std::cerr << "Couldn't load .wav file: " << filename << std::endl;
        return std::optional<std::vector<uint8_t>>();
    }

    char buffer[4];

    file.read(buffer, 4);
    if(std::strncmp(buffer, "RIFF", 4) != 0) {
        std::cerr << "Not a valid .wav file: " << filename << std::endl;
        return std::optional<std::vector<uint8_t>>();
    }

    file.seekg(4, std::ios_base::cur); // file length
    file.read(buffer, 4);

    if(std::strncmp(buffer, "WAVE", 4) != 0) {
        std::cerr << "Not a valid .wav file: " << filename << std::endl;
        return std::optional<std::vector<uint8_t>>();
    }

    while(!file.eof()) {
        file.read(buffer, 4);  // Chunk id

        if (verbosity() > 2) {
            std::cout << "Chunk ID is " << buffer[0] << buffer[1] << buffer[2] << buffer[3] << std::endl;
        }

        if(std::strncmp(buffer, "fmt ", 4) == 0) {

            /* Check the format */
            struct Format {
                uint32_t length;
                uint16_t type;
                uint16_t channels;
                uint32_t frequency;
                uint32_t rate;
                uint16_t bytes_per_sample;
                uint16_t bitrate;
            } format;

            file.read((char*) &format, sizeof(format));

            if (verbosity() > 2) {
                std::cout << std::dec;
                std::cout << "  Length is " << format.length << std::endl;
                std::cout << "  Channels is " << format.channels << std::endl;
                std::cout << "  Frequency is " << format.frequency << std::endl;
            }

            if(format.frequency != 44100 || format.channels != 2 || format.bitrate != 16) {
                std::cerr << "Unsupported .wav format. Must be stereo, 44100hz, and 16 bit samples.: " << filename << std::endl;
                return std::optional<std::vector<uint8_t>>();
            }

        } else if(std::strncmp(buffer, "data", 4) == 0) {

            if (verbosity() > 2) {
                std::cout << "  Data chunk" << std::endl;
            }

            /* Read the data */
            uint32_t length;
            file.read((char*) &length, sizeof(length));
            data.resize(length);
            file.read((char*) &data[0], length);
            break;

        } else {
            uint32_t skip;
            file.read((char*) &skip, sizeof(skip));
            file.seekg(skip, std::ios_base::cur);

            if (verbosity() > 2) {
                std::cout << "  Skipped " << skip << " bytes" << std::endl;
            }
            continue;
        }
    }

    return data;
}

static size_t estimate_padding_sectors(const cd_image_t* image_so_far, const std::filesystem::path& data_dir) {
    auto dir_size = [](const std::filesystem::path& dir) -> std::size_t {
        std::size_t size{ 0 };
        for(const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if(entry.is_regular_file() && !entry.is_symlink()) {
                size += entry.file_size();
            }
        }
        return size;
    };

    const std::size_t iso_padding = 10 * 1024 * 1024;  /* 10M for ISO structures */
    std::size_t used = cd_image_length_in_sectors(image_so_far);
    used += dir_size(data_dir) / 2048;
    used += iso_padding / 2048;

    return std::max(333000 - int32_t(used), 0);
}

static void print_node_info(IsoNode *node, int depth) {
    if(!node) {
        return;
    }

    /* Print indentation */
    for(int i = 0; i < depth; i++) {
        std::cout << "  ";
    }

    const char *name = iso_node_get_name(node);
    if(!name)
        return;

    std::cout << name;

    /* If it's a directory, add a trailing slash */
    if(iso_node_get_type(node) == LIBISO_DIR) {
        std::cout << "/";
    } else if(iso_node_get_type(node) == LIBISO_FILE) {
        /* Retrieve and print sort weight for files */
        IsoFile *file = (IsoFile*)(node);
        if(file) {
            int weight = iso_file_get_sort_weight(file);
            std::cout << " [Weight: " << std::dec << weight << "]";
        }
    }

    std::cout << std::endl;
}

static void traverse_directory(IsoDir *dir, int depth) {
    if(!dir) {
        return;
    }

    IsoNode *node;
    IsoDirIter *iter;

    /* Get directory iterator */
    int result = iso_dir_get_children(dir, &iter);
    if(result < 0) {
        return;
    }

    /* Iterate over directory entries */
    while(iso_dir_iter_next(iter, &node) == 1) {
        if(!node) {
            continue;
        }

        print_node_info(node, depth);

        if(iso_node_get_type(node) == LIBISO_DIR) {
            IsoDir *subdir = (IsoDir*)node;
            traverse_directory(subdir, depth + 1);
        }
    }

    iso_dir_iter_free(iter);
}

std::string construct_path(const std::vector<std::string> &parents, const std::string &current) {
    std::ostringstream full_path;
    
    for(const auto &dir : parents) {
        full_path << "/" << dir;
    }

    if(!current.empty()) {
        full_path << "/" << current;
    }

    return full_path.str();
}

static void traverse_and_set_weights(
    IsoDir *dir,
    std::vector<std::string> parent_dirs = {}
) {
    if(!dir || SORT_WEIGHTS.empty()) {
        return;
    }

    IsoNode *node;
    IsoDirIter *iter;

    /* Get directory iterator */
    int result = iso_dir_get_children(dir, &iter);
    if(result < 0) {
        return;
    }

    /* Iterate over directory entries */
    while(iso_dir_iter_next(iter, &node) == 1) {
        if(!node) {
            continue;
        }

        const char *name = iso_node_get_name(node);
        if(!name) {
            continue;
        }

        std::string current_name(name);
        std::string child_full_path = construct_path(parent_dirs, current_name);

        /* Make sure we give the special 0.0 file at the root the most weight */
        if(!OPTS.count("no-padding") && current_name == "0.0") {
            iso_node_set_sort_weight(node, INT_MAX);
        }

        /* Check if the node has a specific weight */
        auto child_it = SORT_WEIGHTS.find(child_full_path);
        if(child_it != SORT_WEIGHTS.end()) {
            /* Override with specific weight */
            iso_node_set_sort_weight(node, child_it->second);
        }

        /* Recurse into directories */
        if(iso_node_get_type(node) == LIBISO_DIR) {
            /* Add current directory to the parent list and recurse */
            auto updated_parent_dirs = parent_dirs;
            updated_parent_dirs.push_back(current_name);

            traverse_and_set_weights((IsoDir *)node, updated_parent_dirs);
        }
    }

    iso_dir_iter_free(iter);
}

static bool parse_sort_file(const std::string &file_path) {
    std::ifstream file(file_path);

    if(!file.is_open()) {
        std::cerr << "Could not open sort file: " << file_path << std::endl;
        return false;
    }

    std::string line;
    int line_number = 0;

    while(std::getline(file, line)) {
        ++line_number;

        /* Skip empty lines or lines starting with a comment (#) */
        if(line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);
        std::string path;
        int weight;

        /* Extract path and weight from the line */
        if(!(iss >> path >> weight)) {
            if(verbosity() > 2) {
                std::cerr << "Invalid format on line " << line_number << ": " << line << std::endl;
            }
            continue;
        }

        SORT_WEIGHTS[path] = weight;
    }

    file.close();

    return !SORT_WEIGHTS.empty();
}

bool build_cdi(const std::filesystem::path& input_dir) {
    std::string output_cdi = OPTS["output"][0];

    IPBin ip_bin;
    bool use_custom_ipbin = false;

    if (OPTS.count("ipbin")){
        use_custom_ipbin = load_ip_bin(OPTS["ipbin"][0], &ip_bin);
        if (use_custom_ipbin){
            std::cout << "Using custom IP.BIN. Other options related to IP.BIN will be ignored (name, MR Image, ...)." << std::endl;
        } else {
            std::cout << "Using default IP.BIN due to errors." << std::endl;
        }
    }
    if (!use_custom_ipbin){
        generate_ip_bin(input_dir, input_dir / "1ST_READ.BIN", &ip_bin);
    }

    /* Generate CDI image */
    cd_image_t* img = cd_new_image();
    cd_image_set_volume_name(img, PATH_TO_CSTR(std::filesystem::path(output_cdi).filename().stem()));

    /* Add the first session, this is where CDDA tracks go */
    cd_session_t* session0 = cd_new_session(img);

    if(OPTS.count("cdda")) {
        size_t track_idx = 0;
        for(auto& filename: OPTS["cdda"]) {
            ++track_idx;
            auto data_maybe = wav_to_cdda(filename);
            if(data_maybe) {
                auto data = data_maybe.value();

                size_t const audio_sector_size = 2352;
                size_t const audio_bytes_per_second = 176400;
                size_t const minimum_track_sectors = 300;
                size_t const minimum_track_size = minimum_track_sectors * audio_sector_size;
                size_t const minimum_track_duration = minimum_track_size / audio_bytes_per_second;

                /* Enforce audio track size */
                if (data.size() < minimum_track_size) {
                    std::cerr << "Audio file " << filename << " is too short - it must be at least " << minimum_track_size << " bytes (" << minimum_track_duration << " seconds)" << std::endl;
                    return false;
                }

                cd_new_track(session0, TRACK_TYPE_AUDIO, &data[0], data.size());

                if (verbosity() > 0) {
                    std::cout << std::dec << "Added track " << track_idx << " (" << data.size() << " bytes) from " << filename << std::endl;
                }
            } else {
                return false;
            }
        }
    } else {
        cd_new_track_blank(session0, TRACK_TYPE_AUDIO, 2352 * 302); /* 4 seconds of audio */
    }

    size_t start_lba = cd_session_length_in_sectors(session0);

    std::size_t padding_file_size = estimate_padding_sectors(img, input_dir) * 2048;

    if(!OPTS.count("no-padding") && padding_file_size > 0) {
        std::ofstream null_file(std::filesystem::path(input_dir) / "0.0");
        std::vector<uint8_t> null_data(padding_file_size, 0);
        null_file.write((char*) &null_data[0], null_data.size());
        null_file.close();
    }

    /* Build ISO */
    iso_init();

    IsoImage* iso = NULL;
    IsoWriteOpts *opts = NULL;
    IsoDir* root = NULL;

    /* Volume names must be < 32 bytes, but it seems some systems have trouble
     * with that much. Here we just use the first 15 chars of the name + nullbyte
     * means 16. */
    std::string name = OPTS.count("name") ? OPTS["name"][0] : "Dreamcast Game";
    name = name.substr(0, 15);
    iso_image_new(name.c_str(), &iso);

    iso_tree_set_follow_symlinks(iso, 0);
    iso_tree_set_ignore_hidden(iso, 0);
    iso_tree_set_ignore_special(iso, 0);

    root = iso_image_get_root(iso);

    iso_tree_add_dir_rec(iso, root, PATH_TO_CSTR(input_dir));

    if(OPTS.count("sort-file") && parse_sort_file(OPTS["sort-file"][0])) {
        if(verbosity() > 2) {
            traverse_directory(root, 0);
        }

        /* Set root weight if one is assigned */
        auto it = SORT_WEIGHTS.find("/");
        if(it != SORT_WEIGHTS.end()) {
            iso_node_set_sort_weight((IsoNode *)root, it->second);
        }

        traverse_and_set_weights(root);

        if(verbosity() > 2) {
            traverse_directory(root, 0);
        }
    }

    iso_write_opts_new(&opts, 0);
    iso_write_opts_set_joliet(opts, 1);
    iso_write_opts_set_rockridge(opts, 1);
    iso_write_opts_set_system_area(opts, (char*) &ip_bin, 0, 0);
    iso_write_opts_set_ms_block(opts, start_lba);

    struct burn_source* burn_src;
    iso_image_create_burn_source(iso, opts, &burn_src);

    if(verbosity() > 0) {
        std::cout << "Generating data track...    " << std::flush;
    }

    std::vector<uint8_t> iso_data;
    unsigned char buf[2048];
    while (int read = burn_src->read_xt(burn_src, buf, 2048)) {
        iso_data.insert(iso_data.end(), buf, buf + read);
    }

    free(burn_src);
    iso_write_opts_free(opts);

    if(verbosity() > 0) {
        std::cout << "Done." << std::endl;
    }

    if(verbosity() > 2) {
        std::cout << "Data track size: " << iso_data.size() << " bytes" << std::endl;
    }

    if(OPTS.count("dump-iso")) {
        std::string iso_path = PATH_TO_CSTR(std::filesystem::path(output_cdi).replace_extension("iso"));

        if(verbosity() > 0) {
            std::cout << "Dumping .iso to: " << iso_path << std::endl;
        }

        std::ofstream iso_out(iso_path, std::ios::binary);
        iso_out.write((char*) &iso_data[0], iso_data.size());
    }

    cd_session_t* session1 = cd_new_session(img);
    cd_track_t* data = cd_new_track(session1, TRACK_TYPE_DATA, &iso_data[0], iso_data.size());
    cd_track_set_mode(data, TRACK_MODE_XA_MODE2_FORM1);
    cd_track_set_postgap_sectors(data, 2); /* Add 2 sectors of postgap to the data track */

    const char* fname = output_cdi.c_str();
    FILE* output = fopen(fname, "wb");
    cd_write_to_cdi(img, output, fname);
    cd_free_image(&img);
    fclose(output);

    return true;
}

int main(int argc, char* argv[]) {
    bool args_ok = false;
    OPTS = parse_options(argc, argv, &args_ok);

    if(!args_ok || OPTS.count("help")) {
        print_help();
        return int(args_ok);
    }

    if(!OPTS.count("output")) {
        print_help();
        return 2;
    }

    if(!OPTS.count("elf") && !OPTS.count("unscrambled-binary") && !OPTS.count("scrambled-binary")) {
        print_help();
        return 2;
    }

    if(verbosity() > 1) {
        std::cout << "Pre-run checks finished. Beginning generation!" << std::endl;
    }

    auto temp_dir_maybe = generate_temporary_directory();
    if(!temp_dir_maybe) {
        return 4;
    }

    auto temp_dir = temp_dir_maybe.value();

    if(!gather_files(temp_dir)) {
        destroy_temp_directory(temp_dir);
        return 4;
    }

    if(!build_cdi(temp_dir)) {
        std::cout << "Error building CDI" << std::endl;
        destroy_temp_directory(temp_dir);
        return 5;
    }

    destroy_temp_directory(temp_dir);
    return 0;
}
