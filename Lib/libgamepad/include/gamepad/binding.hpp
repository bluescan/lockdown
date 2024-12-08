/**
 ** This file is part of the libgamepad project.
 ** Copyright 2020 univrsal <universailp@web.de>.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU Lesser General Public License as
 ** published by the Free Software Foundation, either version 3 of the
 ** License, or (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU Lesser General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#pragma once

#include <json/json11.hpp>
#include <map>
#include <string>
namespace gamepad {
namespace cfg {
    using mappings = std::map<uint16_t, uint16_t>;

    class binding {
    protected:
        std::string m_binding_name;
        mappings m_buttons_mappings;
        mappings m_axis_mappings;

    public:
        binding() = default;
        binding(const std::string& json);
#ifdef LGP_ENABLE_JSON
        binding(const json11::Json& j);

        virtual bool load(const json11::Json& j);
        virtual void save(json11::Json& j) const;
#endif

        virtual bool load(const std::string& json);
        virtual void save(std::string& json);
        virtual void copy(const std::shared_ptr<binding> other);
        const std::string& get_name() const { return m_binding_name; }
        void set_name(const std::string& name) { m_binding_name = name; }
        mappings& get_button_mappings() { return m_buttons_mappings; }
        mappings& get_axis_mappings() { return m_axis_mappings; }
        const mappings& get_button_mappings() const { return m_buttons_mappings; }
        const mappings& get_axis_mappings() const { return m_axis_mappings; }
    };
}
}
