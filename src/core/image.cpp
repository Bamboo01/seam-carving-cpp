#include "seam/image.hpp"

// stb is third-party C; silence its warnings rather than fix it.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4100 4244 4245 4267 4456 4701 4703 6385 6386)
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace seam
{
namespace
{

std::string failure(const std::string& path, const char* reason)
{
    return "image '" + path + "': " + (reason ? reason : "unknown error");
}

std::string lowercase_extension(const std::string& path)
{
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return {};
    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c)
                   {
                       return static_cast<char>(std::tolower(c));
                   });
    return ext;
}

} // namespace

Image load_image(const std::string& path, int desired_channels)
{
    if (desired_channels < 1 || desired_channels > 4)
    {
        throw std::invalid_argument("desired_channels must be in [1, 4]");
    }

    int width = 0;
    int height = 0;
    int source_channels = 0;
    stbi_uc* data =
        stbi_load(path.c_str(), &width, &height, &source_channels, desired_channels);
    if (data == nullptr)
    {
        throw std::runtime_error(failure(path, stbi_failure_reason()));
    }

    Image image;
    image.width = width;
    image.height = height;
    image.channels = desired_channels;

    const std::size_t count = static_cast<std::size_t>(width) *
                              static_cast<std::size_t>(height) *
                              static_cast<std::size_t>(desired_channels);
    image.pixels.assign(data, data + count);
    stbi_image_free(data);
    return image;
}

void save_png(const Image& image, const std::string& path)
{
    if (image.empty())
        throw std::runtime_error("save_png: image is empty");
    const int stride = image.width * image.channels;
    if (stbi_write_png(path.c_str(), image.width, image.height, image.channels,
                       image.pixels.data(), stride) == 0)
    {
        throw std::runtime_error("save_png: could not write '" + path + "'");
    }
}

void save_jpg(const Image& image, const std::string& path, int quality)
{
    if (image.empty())
        throw std::runtime_error("save_jpg: image is empty");
    const int comp = image.channels == 4 ? 4 : 3;
    if (stbi_write_jpg(path.c_str(), image.width, image.height, comp, image.pixels.data(),
                       quality) == 0)
    {
        throw std::runtime_error("save_jpg: could not write '" + path + "'");
    }
}

void save_image(const Image& image, const std::string& path)
{
    const std::string ext = lowercase_extension(path);
    if (ext == "jpg" || ext == "jpeg")
    {
        save_jpg(image, path);
    }
    else if (ext == "png" || ext == "bmp" || ext == "tga")
    {
        save_png(image, path);
    }
    else
    {
        throw std::invalid_argument("save_image: unsupported extension in '" + path +
                                    "' (use .png or .jpg)");
    }
}

} // namespace seam
