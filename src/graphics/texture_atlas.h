#pragma once

#include <ctime>
#include <map>
#include <string>
#include <vector>
#include <utility>

#include "graphics/image.h"
#include "program/errors.h"
#include "reflection/full.h"
#include "strings/common.h"
#include "utils/filesystem.h"
#include "utils/mat.h"

namespace Graphics
{
    class TextureAtlas
    {
        REFL_SIMPLE_STRUCT_WITHOUT_NAMES( ImageDesc
            REFL_DECL(ivec2) pos, size
        )

        REFL_SIMPLE_STRUCT( Desc
            REFL_DECL(std::map<std::string, ImageDesc>) images
        )

        Image image;
        Desc desc;
        std::string source_dir;

      public:
        struct Region
        {
            ivec2 pos = ivec2(0);
            ivec2 size = ivec2(0);

            Region() {}

            Region region(ivec2 sub_pos, ivec2 sub_size) const
            {
                Region ret;
                ret.pos = pos + sub_pos;
                ret.size = sub_size;
                return ret;
            }

            Region margin(int m) const
            {
                Region ret;
                ret.pos = pos + m;
                ret.size = size - 2 * m;
                return ret;
            }
        };

        class ImageList
        {
            friend class TextureAtlas;

            std::vector<Region> list;

          public:
            ImageList() {}

            // [] wraps around for easier animation.
            Region &operator[](int index)
            {
                return const_cast<Region &>(std::as_const(*this)[index]);
            }
            const Region &operator[](int index) const
            {
                return list[mod_ex(index, int(list.size()))];
            }
        };

        TextureAtlas() {}

        // Pass empty string as `source_dir` to disallow regeneration.
        TextureAtlas(ivec2 target_size, const std::string &source_dir, const std::string &out_image_file, const std::string &out_desc_file, bool add_gaps = 1);

        const std::string &SourceDirectory() const
        {
            return source_dir;
        }

        Image &GetImage()
        {
            return image;
        }
        const Image &GetImage() const
        {
            return image;
        }

        bool GetOpt(const std::string &name, Region &target) const // Returns false if no such image.
        {
            auto it = desc.images.find(name);
            if (it == desc.images.end())
                return false;

            target.pos = it->second.pos;
            target.size = it->second.size;
            return true;
        }

        Region Get(const std::string &name) const
        {
            Region ret;
            if (!GetOpt(name, ret))
                Program::Error("No image `", name, "` in texture atlas for `", source_dir, "`.");
            return ret;
        }
        ImageList GetList(const std::string &prefix, int first_index, const std::string &suffix, int count = -1) const
        {
            ImageList ret;

            int offset = 0;
            while (offset != count)
            {
                int index = first_index + offset;
                std::string name = Str(prefix, index, suffix);

                Region image;
                if (!GetOpt(name, image))
                {
                    if (count < 0)
                        break;
                    Program::Error("Image list `", prefix, '#', suffix, "` from texture atlas for `", source_dir, "` has no image with index ", index, ".");
                }
                ret.list.push_back(std::move(image));

                offset++;
            }

            return ret;
        }
    };
}
