// Copyright (C) 2021 The sold authors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "ldsoconf.h"

#include <glob.h>

#include <fstream>

namespace ldsoconf {
namespace {
void read_ldsoconf_dfs(std::vector<std::string>& res, const std::string& filename) {
    std::ifstream f;
    f.open(filename);
    if (!f) {
        return;
    }
    std::string head;
    while (f >> head) {
        if (head.substr(0, 1) == "#") {
            std::string comment;
            std::getline(f, comment);
        } else if (head == "include") {
            std::string descendants;
            f >> descendants;

            glob_t globbuf;
            glob(descendants.c_str(), 0, NULL, &globbuf);
            for (int i = 0; i < globbuf.gl_pathc; i++) {
                read_ldsoconf_dfs(res, globbuf.gl_pathv[i]);
            }
            globfree(&globbuf);
        } else {
            res.push_back(head);
        }
    }
}

}  // namespace

std::vector<std::string> read_ldsoconf() {
    std::vector<std::string> res;
    read_ldsoconf_dfs(res, "/etc/ld.so.conf");

    return res;
}
}  // namespace ldsoconf
