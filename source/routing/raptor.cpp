#include "raptor.h"
#include <boost/foreach.hpp>

namespace navitia { namespace routing { namespace raptor{

void RAPTOR::make_queue() {
    marked_rp.reset();
    marked_sp.reset();
}



 void RAPTOR::make_queuereverse() {
    marked_rp.reset();
    marked_sp.reset();
 }




void RAPTOR::journey_pattern_path_connections_forward(const bool wheelchair) {
    std::vector<type::idx_t> to_mark;
    for(auto rp = marked_rp.find_first(); rp != marked_rp.npos; rp = marked_rp.find_next(rp)) {
        BOOST_FOREACH(auto &idx_rpc, data.dataRaptor.footpath_rp_forward.equal_range(rp)) {
            const auto & rpc = idx_rpc.second;
            navitia::type::DateTime dt = labels[count][rp].arrival + rpc.length;
            if(labels[count][rp].type == vj && dt < best_labels[rpc.destination_journey_pattern_point_idx].arrival /*&& !wheelchair*/) {
                if(rpc.connection_kind == type::ConnectionKind::extension)
                    labels[count][rpc.destination_journey_pattern_point_idx] = label(dt, dt, rp, connection_extension);
                else
                    labels[count][rpc.destination_journey_pattern_point_idx] = label(dt, dt, rp, connection_guarantee);
                best_labels[rpc.destination_journey_pattern_point_idx] = labels[count][rpc.destination_journey_pattern_point_idx];
                to_mark.push_back(rpc.destination_journey_pattern_point_idx);
            }
        }
    }

    for(auto rp : to_mark) {
        marked_rp.set(rp);
        const auto & journey_pattern_point = data.pt_data.journey_pattern_points[rp];
        if(Q[journey_pattern_point.journey_pattern_idx] > journey_pattern_point.order) {
            Q[journey_pattern_point.journey_pattern_idx] = journey_pattern_point.order;
        }
    }
}




void RAPTOR::journey_pattern_path_connections_backward(const bool wheelchair) {
    std::vector<type::idx_t> to_mark;
    for(auto rp = marked_rp.find_first(); rp != marked_rp.npos; rp = marked_rp.find_next(rp)) {
        BOOST_FOREACH(auto &idx_rpc,  data.dataRaptor.footpath_rp_backward.equal_range(rp)) {
            const auto & rpc = idx_rpc.second;
            navitia::type::DateTime dt = labels[count][rp].arrival - rpc.length;
            if(labels[count][rp].type == vj && dt > best_labels[rpc.departure_journey_pattern_point_idx].arrival /*&& !wheelchair*/) {
                if(rpc.connection_kind == type::ConnectionKind::extension)
                    labels[count][rpc.departure_journey_pattern_point_idx] = label(dt, dt, rp, connection_extension);
                else
                    labels[count][rpc.departure_journey_pattern_point_idx] = label(dt, dt, rp, connection_guarantee);
                best_labels[rpc.departure_journey_pattern_point_idx] = labels[count][rpc.departure_journey_pattern_point_idx];
                to_mark.push_back(rpc.departure_journey_pattern_point_idx);
            }
        }
    }

    for(auto rp : to_mark) {
        marked_rp.set(rp);
        const auto & journey_pattern_point = data.pt_data.journey_pattern_points[rp];
        if(Q[journey_pattern_point.journey_pattern_idx] < journey_pattern_point.order) {
            Q[journey_pattern_point.journey_pattern_idx] = journey_pattern_point.order;
        }
    }
}




struct walking_backwards_visitor {

    static navitia::type::DateTime worst() {
        return navitia::type::DateTime::min;
    }
    template<typename T1, typename T2> bool comp(const T1& a, const T2& b) const {
        return a > b;
    }
    template<typename T1, typename T2> auto combine(const T1& a, const T2& b) const -> decltype(a-b) {
        return a - b;
    }

    static navitia::type::DateTime label::*  instant;
    static constexpr bool clockwise = false;
};
navitia::type::DateTime label::*  walking_backwards_visitor::instant= &label::departure;

struct walking_visitor {
    static navitia::type::DateTime worst() {
        return navitia::type::DateTime::inf;
    }
    template<typename T1, typename T2> bool comp(const T1& a, const T2& b) const {
        return a < b;
    }
    template<typename T1, typename T2> auto combine(const T1& a, const T2& b) const -> decltype(a+b) {
        return a + b;
    }

    static navitia::type::DateTime label::*  instant;
    static constexpr bool clockwise = true;
};
navitia::type::DateTime label::*  walking_visitor::instant = &label::arrival;


template<typename Visitor>
void RAPTOR::foot_path(const Visitor & v, const bool wheelchair) {
    std::vector<navitia::type::Connection>::const_iterator it = (v.clockwise) ? data.dataRaptor.foot_path_forward.begin() :
                                                                                data.dataRaptor.foot_path_backward.begin();
    int last = 0;

    for(auto stop_point_idx = marked_sp.find_first(); stop_point_idx != marked_sp.npos; 
        stop_point_idx = marked_sp.find_next(stop_point_idx)) {
        //On cherche le plus petit rp du sp 
        navitia::type::DateTime best_arrival = v.worst();
        type::idx_t best_rp = navitia::type::invalid_idx;
        const type::StopPoint & stop_point = data.pt_data.stop_points[stop_point_idx];
        /*if(!wheelchair) */{
            for(auto rpidx : stop_point.journey_pattern_point_list) {
                if((labels[count][rpidx].type == vj || labels[count][rpidx].type == depart) && v.comp(labels[count][rpidx].arrival, best_arrival)) {
                    best_arrival = labels[count][rpidx].arrival;
                    best_rp = rpidx;
                }
            }
            if(best_rp != type::invalid_idx) {
                navitia::type::DateTime best_departure = v.combine(best_arrival, 120);
                //On marque tous les journey_pattern points du stop point
                for(auto rpidx : stop_point.journey_pattern_point_list) {
                    if(rpidx != best_rp && v.comp(best_departure, best_labels[rpidx].*Visitor::instant) ) {
                       const label nLavel= label(best_departure, best_departure, best_rp);
                       best_labels[rpidx] = nLavel;
                       labels[count][rpidx] = nLavel;
                       const auto & journey_pattern_point = data.pt_data.journey_pattern_points[rpidx];
                       if(!b_dest.add_best(rpidx, nLavel, count, v.clockwise) && v.comp(journey_pattern_point.order, Q[journey_pattern_point.journey_pattern_idx]) ) {
        //                   marked_rp.set(rpidx);
                           Q[journey_pattern_point.journey_pattern_idx] = journey_pattern_point.order;
                       }
                    }
                }


                //On va maintenant chercher toutes les connexions et on marque tous les journey_pattern_points concernés

                const pair_int & index = (v.clockwise) ? data.dataRaptor.footpath_index_forward[stop_point_idx] :
                                                         data.dataRaptor.footpath_index_backward[stop_point_idx];

                const label & label_temp = labels[count][best_rp];
                int prec_duration = -1;
                navitia::type::DateTime next = v.worst(), previous = label_temp.*v.instant;
                it += index.first - last;
                const auto end = it + index.second;

                for(; it != end; ++it) {
                    navitia::type::idx_t destination = (*it).destination_stop_point_idx;
                    /*if(!wheelchair)*/ {
                        for(auto destination_rp : data.pt_data.stop_points[destination].journey_pattern_point_list) {
                            if(best_rp != destination_rp) {
                                if(it->duration != prec_duration) {
                                    next = v.combine(previous, it->duration);
                                    prec_duration = it->duration;
                                }
                                if(v.comp(next, best_labels[destination_rp].*v.instant) || next == best_labels[destination_rp].*v.instant) {
                                    const label nLabel = label(next, next, best_rp);
                                    best_labels[destination_rp] = nLabel;
                                    labels[count][destination_rp] = nLabel;
                                    const auto & journey_pattern_point = data.pt_data.journey_pattern_points[destination_rp];

                                   if(!b_dest.add_best(destination_rp, nLabel, count, v.clockwise) && v.comp(journey_pattern_point.order, Q[journey_pattern_point.journey_pattern_idx])) {
            //                            marked_rp.set(destination_rp);
                                        Q[journey_pattern_point.journey_pattern_idx] = journey_pattern_point.order;
                                   }
                                }
                            }
                        }
                    }
                }

                 last = index.first + index.second;
            }
        }


     }
}



void RAPTOR::marcheapied(const bool wheelchair) {
    walking_visitor v;
    foot_path(v, wheelchair);
}

void RAPTOR::marcheapiedreverse(const bool wheelchair) {
    walking_backwards_visitor v;
    foot_path(v, wheelchair);
}



void RAPTOR::clear_and_init(std::vector<init::Departure_Type> departs,
                  std::vector<std::pair<type::idx_t, double> > destinations,
                  navitia::type::DateTime borne,  const bool clockwise, const bool clear,
                  const float walking_speed, const int walking_distance) {

    if(clockwise)
        Q.assign(data.pt_data.journey_patterns.size(), std::numeric_limits<int>::max());
    else {
        Q.assign(data.pt_data.journey_patterns.size(), -1);
    }

    if(clear) {
        labels.clear();
        best_labels.clear();
        if(clockwise) {
            labels.push_back(data.dataRaptor.labels_const);
            best_labels = data.dataRaptor.labels_const;
            b_dest.reinit(data.pt_data.journey_pattern_points.size(), borne, clockwise, std::ceil(walking_distance/walking_speed));
        } else {
            labels.push_back(data.dataRaptor.labels_const_reverse);
            best_labels = data.dataRaptor.labels_const_reverse;
            if(borne == navitia::type::DateTime::inf)
                borne = navitia::type::DateTime::min;
                b_dest.reinit(data.pt_data.journey_pattern_points.size(), borne, clockwise, std::ceil(walking_distance/walking_speed));
        }
    }


    for(init::Departure_Type item : departs) {
        labels[0][item.rpidx] = label(item.arrival, item.arrival);
        best_labels[item.rpidx] = labels[0][item.rpidx];
        const auto & journey_pattern_point = data.pt_data.journey_pattern_points[item.rpidx];
        if(clockwise && Q[journey_pattern_point.journey_pattern_idx] > journey_pattern_point.order)
            Q[journey_pattern_point.journey_pattern_idx] = journey_pattern_point.order;
        else if(!clockwise &&  Q[journey_pattern_point.journey_pattern_idx] < journey_pattern_point.order)
            Q[journey_pattern_point.journey_pattern_idx] = journey_pattern_point.order;
        if(item.arrival != navitia::type::DateTime::min && item.arrival != type::DateTime::inf) {
            marked_sp.set(data.pt_data.journey_pattern_points[item.rpidx].stop_point_idx);
        }
    }

    for(auto item : destinations) {
        for(type::idx_t rpidx: data.pt_data.stop_points[item.first].journey_pattern_point_list) {
            if(journey_patterns_valides.test(data.pt_data.journey_pattern_points[rpidx].journey_pattern_idx) &&
                    ((clockwise && (borne == navitia::type::DateTime::inf || best_labels[rpidx].arrival > borne)) ||
                    ((!clockwise) && (borne == navitia::type::DateTime::min || best_labels[rpidx].departure < borne)))) {
                    b_dest.add_destination(rpidx, (item.second/walking_speed));
                    if(clockwise)
                        best_labels[rpidx].arrival = borne;
                    else
                        best_labels[rpidx].departure = borne;
                }
        }
    }

     count = 1;
}


template<typename Visitor>
std::vector<Path>
RAPTOR::compute_all(const std::vector<std::pair<type::idx_t, double> > &departs,
                    const std::vector<std::pair<type::idx_t, double> > &destinations,
                    const navitia::type::DateTime &dt_depart, const navitia::type::DateTime &borne, const float walking_speed,
                    const int walking_distance, const bool wheelchair, const std::multimap<std::string, std::string> & forbidden,
                    Visitor vis) {
    std::vector<Path> result;
    std::vector<init::Departure_Type> departures;

    set_journey_patterns_valides(dt_depart.date(), forbidden);
    departures = init::getDepartures(departs, dt_depart, vis.first_phase_clockwise, data, walking_speed);
    clear_and_init(departures, destinations, borne, vis.first_phase_clockwise, true, walking_speed, walking_distance);
    if(vis.first_phase_clockwise) {
        boucleRAPTOR(wheelchair/*, false*/);
    } else {
        boucleRAPTORreverse(wheelchair/*, false*/);
    }


    if(b_dest.best_now.type == uninitialized) {
        return result;
    }

    //Second passe
    if(vis.second_phase_clockwise)
        departures = init::getDepartures(destinations, departs, vis.second_phase_clockwise, this->labels, data, walking_speed);
    else
        departures = init::getDepartures(departs, destinations, vis.second_phase_clockwise, this->labels, data, walking_speed);

    for(auto departure : departures) {
        if(vis.second_phase_clockwise) {
            clear_and_init({departure}, destinations, dt_depart, vis.second_phase_clockwise, true, walking_speed, walking_distance);
            boucleRAPTOR(wheelchair, false);
        }
        else {
            clear_and_init({departure}, departs, dt_depart, vis.second_phase_clockwise, true, walking_speed, walking_distance);
            boucleRAPTORreverse(wheelchair, true);
        }

        if(b_dest.best_now.type != uninitialized) {
            std::vector<Path> temp;
            if(vis.second_phase_clockwise)
                temp = makePathes(destinations, dt_depart, walking_speed, *this);
            else
                temp = makePathesreverse(departs, dt_depart, walking_speed, *this);
            result.insert(result.end(), temp.begin(), temp.end());
        }
    }

    return result;
}


struct visitor_clockwise {
    static constexpr bool first_phase_clockwise = true;
    static constexpr bool second_phase_clockwise = false;
};

std::vector<Path>
RAPTOR::compute_all(const std::vector<std::pair<type::idx_t, double> > &departs,
                    const std::vector<std::pair<type::idx_t, double> > &destinations,
                    const navitia::type::DateTime &dt_depart, const navitia::type::DateTime &borne,
                    const float walking_speed, const int walking_distance, const bool wheelchair,
                    const std::multimap<std::string, std::string> & forbidden) {

    return compute_all(departs, destinations, dt_depart, borne, walking_speed, walking_distance,
                       wheelchair, forbidden, visitor_clockwise());
}

struct visitor_anti_clockwise {
    static constexpr bool first_phase_clockwise = false;
    static constexpr bool second_phase_clockwise = true;
};
std::vector<Path>
RAPTOR::compute_reverse_all(const std::vector<std::pair<type::idx_t, double> > &departs,
                            const std::vector<std::pair<type::idx_t, double> > &destinations,
                            const navitia::type::DateTime &dt_depart, const navitia::type::DateTime &borne,
                            float walking_speed, int walking_distance, bool wheelchair,
                            const std::multimap<std::string, std::string> & forbidden) {
    return compute_all(departs, destinations, dt_depart, borne, walking_speed, walking_distance,
                       wheelchair, forbidden, visitor_anti_clockwise());
}



std::vector<Path>
RAPTOR::compute_all(const std::vector<std::pair<type::idx_t, double> > &departs,
                    const std::vector<std::pair<type::idx_t, double> > &destinations,
                    std::vector<navitia::type::DateTime> dt_departs, const navitia::type::DateTime &borne,
                    float walking_speed, int walking_distance, bool wheelchair) {
    std::vector<Path> result;
    std::vector<best_dest> bests;

    std::sort(dt_departs.begin(), dt_departs.end(), 
              [](navitia::type::DateTime dt1, navitia::type::DateTime dt2) {return dt1 > dt2;});

    // Attention ! Et si les départs ne sont pas le même jour ?
    //set_journey_patterns_valides(dt_departs.front());

    bool reset = true;
    for(auto dep : dt_departs) {
        auto departures = init::getDepartures(departs, dep, true, data, walking_speed);
        clear_and_init(departures, destinations, borne, true, reset, walking_speed, walking_distance);
        boucleRAPTOR(wheelchair);
        bests.push_back(b_dest);
        reset = false;
    }

    for(unsigned int i=0; i<dt_departs.size(); ++i) {
        auto b = bests[i];
        auto dt_depart = dt_departs[i];
        if(b.best_now.type != uninitialized) {
            init::Departure_Type d;
            d.rpidx = b.best_now_rpid;
            d.arrival = b.best_now.arrival;
            clear_and_init({d}, departs, dt_depart, false, true, walking_speed, walking_distance);
            boucleRAPTORreverse(wheelchair);
            if(b_dest.best_now.type != uninitialized){
                auto temp = makePathesreverse(departs, dt_depart, walking_speed, *this);
                auto path = temp.back();
                path.request_time = boost::posix_time::ptime(data.meta.production_date.begin(),
                boost::posix_time::seconds(dt_depart.hour()));
                result.push_back(path);
            }
        }
    }

    return result;
}



std::vector<Path>
RAPTOR::compute_reverse_all(const std::vector<std::pair<type::idx_t, double> > &departs,
                    const std::vector<std::pair<type::idx_t, double> > &destinations,
                    std::vector<navitia::type::DateTime> dt_departs, const navitia::type::DateTime &borne,
                    const float walking_speed,  const int walking_distance, const bool wheelchair) {
    std::vector<Path> result;
    std::vector<best_dest> bests;

    std::sort(dt_departs.begin(), dt_departs.end());

    // Attention ! Et si les départs ne sont pas le même jour ?
   // set_journey_patterns_valides(dt_departs.front().date);

    bool reset = true;
    for(auto dep : dt_departs) {
        auto departures = init::getDepartures(departs, dep, false, data, walking_speed);
        clear_and_init(departures, destinations, borne, false, reset, walking_speed, walking_distance);
        boucleRAPTORreverse(wheelchair);
        bests.push_back(b_dest);
        reset = false;
    }

    for(unsigned int i=0; i<dt_departs.size(); ++i) {
        auto b = bests[i];
        auto dt_depart = dt_departs[i];
        if(b.best_now.type != uninitialized) {
            init::Departure_Type d;
            d.rpidx = b.best_now_rpid;
            d.arrival = b.best_now.departure;
            clear_and_init({d}, departs, dt_depart, true, true, walking_speed, walking_distance);
            boucleRAPTOR(wheelchair);
            if(b_dest.best_now.type != uninitialized){
                auto temp = makePathes(destinations, dt_depart, walking_speed, *this);
                auto path = temp.back();
                path.request_time = boost::posix_time::ptime(data.meta.production_date.begin(),
                                                             boost::posix_time::seconds(dt_depart.hour()));
                result.push_back(path);
            }
        }
    }

    return result;
}






void
RAPTOR::isochrone(const std::vector<std::pair<type::idx_t, double> > &departs,
          const navitia::type::DateTime &dt_depart, const navitia::type::DateTime &borne,
          float walking_speed, int walking_distance, bool wheelchair,
          const std::multimap<std::string, std::string> & forbidden,
          bool clockwise) {

    std::vector<idx_label> result;
    set_journey_patterns_valides(dt_depart.date(), forbidden);
    auto departures = init::getDepartures(departs, dt_depart, true, data, walking_speed);
    clear_and_init(departures, {}, borne, true, true, walking_speed, walking_distance);

    if(clockwise) {
        boucleRAPTOR(wheelchair);
    } else {
        boucleRAPTORreverse(wheelchair);
    }
}




void RAPTOR::set_journey_patterns_valides(uint32_t date, const std::multimap<std::string, std::string> & forbidden) {
    journey_patterns_valides.clear();
    journey_patterns_valides.resize(data.pt_data.journey_patterns.size());
    for(const auto & journey_pattern : data.pt_data.journey_patterns) {
        const navitia::type::Route & route = data.pt_data.routes[journey_pattern.route_idx];
        const navitia::type::Line & line = data.pt_data.lines[route.line_idx];
        const navitia::type::CommercialMode & mode = data.pt_data.commercial_modes[journey_pattern.commercial_mode_idx];

        // On gère la liste des interdits
        bool forbidden_journey_pattern = false;
        for(auto pair : forbidden){
            if(pair.first == "line" && pair.second == line.uri)
                forbidden_journey_pattern = true;
            if(pair.first == "journey_pattern" && pair.second == journey_pattern.uri)
                forbidden_journey_pattern = true;
            if(pair.first == "mode" && pair.second == mode.uri)
                forbidden_journey_pattern = true;
        }

        // Si la journey_pattern n'a pas été bloquée par un paramètre, on la valide s'il y a au moins une circulation à j-1/j+1
        if(!forbidden_journey_pattern){
            for(auto vjidx : journey_pattern.vehicle_journey_list) {
                if(data.pt_data.validity_patterns[data.pt_data.vehicle_journeys[vjidx].validity_pattern_idx].check2(date)) {
                    journey_patterns_valides.set(journey_pattern.idx);
                    break;
                }
            }
        }
    }
}



struct raptor_visitor {
    RAPTOR & raptor;
    raptor_visitor(RAPTOR & raptor) : raptor(raptor) {}
    int embarquement_init() const {
        return type::invalid_idx;
    }

    navitia::type::DateTime working_datetime_init() const {
        return navitia::type::DateTime::inf;
    }

    bool better(const navitia::type::DateTime &a, const navitia::type::DateTime &b) const {
        return a < b;
    }

    bool better(const label &a, const label &b) const {
        return a.arrival < b.arrival;
    }

    bool better_or_equal(const label &a, const navitia::type::DateTime &current_dt, const type::StopTime &st) {
        return previous_datetime(a) <= current_datetime(current_dt.date(), st);
    }

    void walking(const bool wheelchair){
        raptor.marcheapied(wheelchair);
    }

    void journey_pattern_path_connections(const bool wheelchair) {
        raptor.journey_pattern_path_connections_forward(wheelchair);
    }

    void make_queue(){
        raptor.make_queue();
    }

    std::pair<type::idx_t, uint32_t> best_trip(const type::JourneyPattern & journey_pattern, int order, const navitia::type::DateTime & date_time, const bool wheelchair) const {
        return earliest_trip(journey_pattern, order, date_time, raptor.data, wheelchair);
    }

    void one_more_step() {
        raptor.labels.push_back(raptor.data.dataRaptor.labels_const);
    }

    bool store_better(const navitia::type::idx_t rpid, navitia::type::DateTime &working_date_time, const label&bound,
                      const navitia::type::StopTime &st, const navitia::type::idx_t embarquement, uint32_t gap) {
        auto & working_labels= raptor.labels[raptor.count];
        working_date_time.update(!st.is_frequency()? st.arrival_time : st.start_time + gap);
        if(better(working_date_time, bound.arrival) && st.drop_off_allowed()) {
            working_labels[rpid] = label(st, working_date_time, embarquement, true, gap);
            raptor.best_labels[rpid] = working_labels[rpid];
            if(!raptor.b_dest.add_best(rpid, working_labels[rpid], raptor.count)) {
                raptor.marked_rp.set(rpid);
                raptor.marked_sp.set(raptor.data.pt_data.journey_pattern_points[rpid].stop_point_idx);
                return false;
            }
        } else if(working_date_time == bound.arrival &&
                  raptor.labels[raptor.count-1][rpid].type == uninitialized) {
            auto l = label(st, working_date_time, embarquement, true, gap);
            if(raptor.b_dest.add_best(rpid, l, raptor.count)) {
                working_labels[rpid] = l;
                raptor.best_labels[rpid] = l;
            }
        }
        return true;
    }

    navitia::type::DateTime previous_datetime(const label & tau) const {
        return tau.arrival;
    }

    navitia::type::DateTime current_datetime(int date, const type::StopTime & stop_time) const {
        return navitia::type::DateTime(date, stop_time.departure_time);
    }

    std::pair<std::vector<type::JourneyPatternPoint>::const_iterator, std::vector<type::JourneyPatternPoint>::const_iterator>
    journey_pattern_points(const type::JourneyPattern &journey_pattern, size_t order) const {
        return std::make_pair(raptor.data.pt_data.journey_pattern_points.begin() + journey_pattern.journey_pattern_point_list[order],
                              raptor.data.pt_data.journey_pattern_points.begin() + journey_pattern.journey_pattern_point_list.back() + 1);
    }

    std::vector<type::StopTime>::const_iterator first_stoptime(size_t position) const {
        return raptor.data.pt_data.stop_times.begin() + position;
    }


    int min_time_to_wait(navitia::type::idx_t /*rp1*/, navitia::type::idx_t /*rp2*/) {
        return 120;
    }

    void update(navitia::type::DateTime & dt, const type::StopTime & st, const uint32_t gap) {
        dt.update(!st.is_frequency()? st.arrival_time : st.start_time+gap);
    }

    void reset_queue_item(int &item) {
        item = std::numeric_limits<int>::max();
    }
};

struct raptor_reverse_visitor {
    RAPTOR & raptor;
    raptor_reverse_visitor(RAPTOR & raptor) : raptor(raptor) {}

    int embarquement_init() const {
        return std::numeric_limits<int>::max();
    }

    navitia::type::DateTime working_datetime_init() const {
        return navitia::type::DateTime::min;
    }

    bool better(const navitia::type::DateTime &a, const navitia::type::DateTime &b) const {
        return a > b;
    }

    bool better(const label &a, const label &b) const {
        return a.departure > b.departure;
    }

    bool better_or_equal(const label &a, const navitia::type::DateTime &current_dt, const type::StopTime &st) {
        return previous_datetime(a) >= current_datetime(current_dt.date(), st);
    }

    void walking(const bool wheelchair) {
        raptor.marcheapiedreverse(wheelchair);
    }

    void journey_pattern_path_connections(const bool wheelchair) {
        raptor.journey_pattern_path_connections_backward(wheelchair);
    }

    void make_queue(){
        raptor.make_queuereverse();
    }

    std::pair<type::idx_t, uint32_t> best_trip(const type::JourneyPattern & journey_pattern, int order, const navitia::type::DateTime & date_time, const bool wheelchair) const {
        return tardiest_trip(journey_pattern, order, date_time, raptor.data, wheelchair);
    }

    void one_more_step() {
        raptor.labels.push_back(raptor.data.dataRaptor.labels_const_reverse);
    }

    bool store_better(const navitia::type::idx_t rpid, navitia::type::DateTime &working_date_time, const label&bound,
                      const navitia::type::StopTime st, const navitia::type::idx_t embarquement, uint32_t gap) {
        auto & working_labels = raptor.labels[raptor.count];
        working_date_time.updatereverse(!st.is_frequency()? st.departure_time : st.start_time + gap);
        if(better(working_date_time, bound.departure) && st.pick_up_allowed()) {
            working_labels [rpid] = label(st, working_date_time, embarquement, false, gap);
            raptor.best_labels[rpid] = working_labels [rpid];
            if(!raptor.b_dest.add_best_reverse(rpid, working_labels [rpid], raptor.count)) {
                raptor.marked_rp.set(rpid);
                raptor.marked_sp.set(raptor.data.pt_data.journey_pattern_points[rpid].stop_point_idx);
                return false;
            }
        } else if(working_date_time == bound.departure &&
                  raptor.labels[raptor.count-1][rpid].type == uninitialized) {
            auto r = label(st, working_date_time, embarquement, false, gap);
            if(raptor.b_dest.add_best_reverse(rpid, r, raptor.count))
                working_labels [rpid] = r;
        }
        return true;
    }

    navitia::type::DateTime previous_datetime(const label & tau) const {
        return tau.departure;
    }

    navitia::type::DateTime current_datetime(int date, const type::StopTime & stop_time) const {
        return navitia::type::DateTime(date, stop_time.arrival_time);
    }

    std::pair<std::vector<type::JourneyPatternPoint>::const_reverse_iterator, std::vector<type::JourneyPatternPoint>::const_reverse_iterator>
    journey_pattern_points(const type::JourneyPattern &journey_pattern, size_t order) const {
        const auto begin = raptor.data.pt_data.journey_pattern_points.rbegin() + raptor.data.pt_data.journey_pattern_points.size() - journey_pattern.journey_pattern_point_list[order] - 1;
        const auto end = begin + order + 1;
        return std::make_pair(begin, end);
    }

    std::vector<type::StopTime>::const_reverse_iterator first_stoptime(size_t position) const {
        return raptor.data.pt_data.stop_times.rbegin() + raptor.data.pt_data.stop_times.size() - position - 1;
    }

    int min_time_to_wait(navitia::type::idx_t /*rp1*/, navitia::type::idx_t /*rp2*/) {
        return -120;
    }

    void update(navitia::type::DateTime & dt, const type::StopTime & st, const uint32_t gap) {
        dt.updatereverse(!st.is_frequency() ? st.departure_time : st.start_time + gap);
    }

    void reset_queue_item(int &item) {
        item = -1;
    }
};

template<typename Visitor>
void RAPTOR::raptor_loop(Visitor visitor, const bool wheelchair, bool global_pruning) {
    bool end = false;
    count = 0;
    type::idx_t t=-1;
    int embarquement = visitor.embarquement_init();
    navitia::type::DateTime workingDt = visitor.working_datetime_init();
    uint32_t l_zone = std::numeric_limits<uint32_t>::max();

    visitor.walking(wheelchair);
    while(!end) {
        ++count;
        end = true;
        if(count == labels.size())
            visitor.one_more_step();
        const auto & prec_labels= labels[count -1];
        visitor.make_queue();
        for(const auto & journey_pattern : data.pt_data.journey_patterns) {
            if(Q[journey_pattern.idx] != std::numeric_limits<int>::max() && Q[journey_pattern.idx] != -1 && journey_patterns_valides.test(journey_pattern.idx)) {
                t = type::invalid_idx;
                embarquement = visitor.embarquement_init();
                workingDt = visitor.working_datetime_init();
                decltype(visitor.first_stoptime(0)) it_st;
                int gap = 0;
                BOOST_FOREACH(const type::JourneyPatternPoint & rp, visitor.journey_pattern_points(journey_pattern, Q[journey_pattern.idx])) {
                    if(t != type::invalid_idx) {
                        ++it_st;
                        if(l_zone == std::numeric_limits<uint32_t>::max() || l_zone != it_st->local_traffic_zone) {
                            //On stocke, et on marque pour explorer par la suite
                            if(visitor.better(best_labels[rp.idx], b_dest.best_now) || !global_pruning)
                                end = visitor.store_better(rp.idx, workingDt, best_labels[rp.idx], *it_st, embarquement, gap) && end;
                            else
                                end = visitor.store_better(rp.idx, workingDt, b_dest.best_now, *it_st, embarquement, gap) && end;
                        }
                    }

                    //Si on peut arriver plus tôt à l'arrêt en passant par une autre journey_pattern
                    const label & labels_temp = prec_labels[rp.idx];
                    if(labels_temp.type != uninitialized &&
                       (t == type::invalid_idx || visitor.better_or_equal(labels_temp, workingDt, *it_st))) {

                        type::idx_t etemp;
                        std::tie(etemp, gap) = visitor.best_trip(journey_pattern, rp.order, visitor.previous_datetime(labels_temp), wheelchair);

                        if(etemp != type::invalid_idx && t != etemp) {
                            t = etemp;
                            embarquement = rp.idx;
                            it_st = visitor.first_stoptime(data.pt_data.vehicle_journeys[t].stop_time_list[rp.order]);
                            workingDt = visitor.previous_datetime(labels_temp);
                            visitor.update(workingDt, *it_st, gap);
                            l_zone = it_st->local_traffic_zone;
                        }
                    }
                }
            }
            visitor.reset_queue_item(Q[journey_pattern.idx]);
        }

        // Prolongements de service
        visitor.journey_pattern_path_connections(wheelchair);

        // Correspondances
        visitor.walking(wheelchair);
    }
}

void RAPTOR::boucleRAPTOR(const bool wheelchair, bool global_pruning){
    auto visitor = raptor_visitor(*this);
    raptor_loop(visitor, wheelchair, global_pruning);
}

void RAPTOR::boucleRAPTORreverse(const bool wheelchair, bool global_pruning){
    auto visitor = raptor_reverse_visitor(*this);
    raptor_loop(visitor, wheelchair, global_pruning);
}







std::vector<Path> RAPTOR::compute(idx_t departure_idx, idx_t destination_idx, int departure_hour,
                                  int departure_day, bool clockwise, bool wheelchair) {
    if(clockwise)
        return compute(departure_idx, destination_idx, departure_hour, departure_day, navitia::type::DateTime::inf, clockwise, wheelchair);
    else
        return compute(departure_idx, destination_idx, departure_hour, departure_day, navitia::type::DateTime::min, clockwise, wheelchair);

}







std::vector<Path> RAPTOR::compute(idx_t departure_idx, idx_t destination_idx, int departure_hour,
                                  int departure_day, navitia::type::DateTime borne, bool clockwise, bool wheelchair) {
    
    std::vector<std::pair<type::idx_t, double> > departs, destinations;

    for(navitia::type::idx_t spidx : data.pt_data.stop_areas[departure_idx].stop_point_list) {
        departs.push_back(std::make_pair(spidx, 0));
    }

    for(navitia::type::idx_t spidx : data.pt_data.stop_areas[destination_idx].stop_point_list) {
        destinations.push_back(std::make_pair(spidx, 0));
    }

    if(clockwise)
        return compute_all(departs, destinations, navitia::type::DateTime(departure_day, departure_hour), borne, 1, 1000, wheelchair);
    else
        return compute_reverse_all(departs, destinations, navitia::type::DateTime(departure_day, departure_hour), borne, 1, 1000, wheelchair);
}


int RAPTOR::best_round(type::idx_t journey_pattern_point_idx){
    for(size_t i = 0; i < labels.size(); ++i){
        if(labels[i][journey_pattern_point_idx].arrival == best_labels[journey_pattern_point_idx].arrival
                && labels[i][journey_pattern_point_idx].departure == best_labels[journey_pattern_point_idx].departure){
            return i;
        }
    }
    return -1;
}

}}}
