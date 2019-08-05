/* Copyright © 2001-2014, Canal TP and/or its affiliates. All rights reserved.

This file is part of Navitia,
    the software to build cool stuff with public transport.

Hope you'll enjoy and contribute to this project,
    powered by Canal TP (www.canaltp.fr).
Help us simplify mobility and open public transport:
    a non ending quest to the responsive locomotion way of traveling!

LICENCE: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

Stay tuned using
twitter @navitia
IRC #navitia on freenode
https://groups.google.com/d/forum/navitia
www.navitia.io
*/
#include <fstream>
#include <boost/program_options.hpp>

#include "type/stop_point.h"
#include "utils/init.h"  // init_app()
#include "type/data.h"
#include "type/pb_converter.h"

using namespace navitia;

namespace po = boost::program_options;
namespace nt = navitia::type;

struct SP {
    nt::StopPoint* stop_point;
    std::vector<nt::StopTime*> st;
};

std::pair<SP, std::vector<SP>> choose_best_sp(std::map<nt::StopPoint*, SP> sps) {
    SP best;
    std::vector<SP> others;
    size_t max = 0;
    for (auto p : sps) {
        if (p.second.st.size() > max) {
            best = p.second;
            max = best.st.size();
        }
    }
    for (auto p : sps) {
        if (p.second.stop_point != best.stop_point) {
            others.push_back(p.second);
        }
    }
    return std::make_pair(best, others);
}

void merge_sp(nt::StopPoint* best, nt::StopPoint* other, nt::Data& data) {
    // merge codes
    auto codes = data.pt_data->codes.get_codes(other);
    for (auto code : codes) {
        for (auto v : code.second) {
            data.pt_data->codes.add(best, code.first, v);
        }
    }
}

void move_st(std::vector<nt::StopTime*> sts, nt::StopPoint* sp) {
    for (auto* st : sts) {
        st->stop_point = sp;
    }
}

int main(int argc, char** argv) {
    navitia::init_app();
    po::options_description desc("options of dump streetnetwork");
    std::string file, output;

    auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
    logger.setLogLevel(log4cplus::WARN_LOG_LEVEL);

    desc.add_options()("help", "Show this message")(
        "file,f", po::value<std::string>(&file)->default_value("data.nav.lz4"), "Path to data.nav.lz4")(
        "output,o", po::value<std::string>(&output)->default_value("newdata.nav.lz4"), "Path to data.nav.lz4");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << "" << std::endl;
        std::cout << desc << std::endl;
        return 0;
    }

    LOG4CPLUS_INFO(logger, "loading data");
    type::Data data;
    data.load_nav(file);

    for (auto* route : data.pt_data->routes) {
        std::map<nt::StopArea*, std::map<nt::StopPoint*, SP>> sa_sp;
        for (auto* vj : route->discrete_vehicle_journey_list) {
            for (auto& st : vj->stop_time_list) {
                SP& sp = sa_sp[st.stop_point->stop_area][st.stop_point];
                sp.stop_point = st.stop_point;
                sp.st.push_back(&st);
            }
        }
        for (auto p : sa_sp) {
            if (p.second.size() > 1) {
                std::cout << "sa duplicated " << p.first->name << " (" << p.first->uri << ") on route " << route->uri
                          << " line " << route->line->code << " (" << route->line->uri << "):" << std::endl;
                for (auto sp : p.second) {
                    std::cout << "\t" << sp.first->name << " - " << sp.first->uri << std::endl;
                }
                SP best;
                std::vector<SP> others;
                std::tie(best, others) = choose_best_sp(p.second);
                for (auto other : others) {
                    merge_sp(best.stop_point, other.stop_point, data);
                    move_st(other.st, best.stop_point);
                }
            }
        }
    }
    data.save(output);
    return 0;
}
