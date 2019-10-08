# encoding: utf-8

# Copyright (c) 2001-2014, Canal TP and/or its affiliates. All rights reserved.
#
# This file is part of Navitia,
#     the software to build cool stuff with public transport.
#
# Hope you'll enjoy and contribute to this project,
#     powered by Canal TP (www.canaltp.fr).
# Help us simplify mobility and open public transport:
#     a non ending quest to the responsive locomotion way of traveling!
#
# LICENCE: This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# Stay tuned using
# twitter @navitia
# IRC #navitia on freenode
# https://groups.google.com/d/forum/navitia
# www.navitia.io
from __future__ import absolute_import, print_function, unicode_literals, division
import logging

from .tests_mechanism import AbstractTestFixture, dataset
from .check_utils import *


@dataset({"main_routing_test": {}})
class TestPlaces(AbstractTestFixture):
    """
    Test places responses
    """

    def test_places_by_id(self):
        """can we get the complete address from coords"""
        # we transform x,y to lon,lat using N_M_TO_DEG constant
        lon = 10.0 / 111319.9
        lat = 100.0 / 111319.9
        response = self.query_region("places/{};{}".format(lon, lat))

        assert len(response['places']) == 1
        is_valid_places(response['places'])
        assert response['places'][0]['name'] == "42 rue kb (Condom)"

    def test_label_of_admin(self):
        """ test label of admin "Condom (03430)" """
        response = self.query_region("places?q=Condom&type[]=administrative_region")

        assert len(response['places']) == 1
        is_valid_places(response['places'])
        assert response['places'][0]['name'] == "Condom (03430)"

    def test_places_invalid_encoding(self):
        _, status = self.query_no_assert(u'/v1/coverage/main_routing_test/places/?q=ch\xe2teau'.encode('utf-8'))
        assert status != 500

    def test_places_do_not_loose_precision(self):
        """do we have a good precision given back in the id"""

        # it should work for any id with 15 digits max on each coords
        # that returns a result
        id = "8.9831195195e-05;0.000898311281954"
        response = self.query_region("places/{}".format(id))

        assert len(response['places']) == 1
        is_valid_places(response['places'])
        assert response['places'][0]['id'] == id
        assert response['places'][0]['address']['id'] == id

    def test_places_nearby(self):
        """check places_nearby"""

        id = "8.9831195195e-05;0.000898311281954"
        response = self.query_region("places/{}/places_nearby".format(id), display=True)

        assert len(response['places_nearby']) > 0
        is_valid_places(response['places_nearby'])

    def test_places_nearby_with_coord(self):
        """check places_nearby with /coord"""

        id = "8.9831195195e-05;0.000898311281954"
        response = self.query_region("coord/{}/places_nearby".format(id))

        assert len(response['places_nearby']) > 0
        is_valid_places(response['places_nearby'])

    def test_places_nearby_with_coords(self):
        """check places_nearby with /coords"""

        id = "8.9831195195e-05;0.000898311281954"
        response = self.query_region("coords/{}/places_nearby".format(id))

        assert len(response['places_nearby']) > 0
        is_valid_places(response['places_nearby'])

        assert len(response['disruptions']) == 0

    def test_places_nearby_with_coord_without_region(self):
        """check places_nearby with /coord"""
        response = self.query("/v1/coord/0.000001;0.000898311281954/places_nearby")
        assert len(response['places_nearby']) > 0
        is_valid_places(response['places_nearby'])

    def test_places_nearby_with_coords_without_region(self):
        """check places_nearby with /coords"""

        response = self.query("/v1/coords/0.000001;0.000898311281954/places_nearby")
        assert len(response['places_nearby']) > 0
        is_valid_places(response['places_nearby'])

    def test_places_nearby_with_coords_without_region_and_type(self):
        """check places_nearby with /coords and type[]=stop_area"""

        response = self.query("/v1/coords/0.000001;0.000898311281954/places_nearby?type[]=stop_area")
        places_nearby = response['places_nearby']
        assert len(places_nearby) == 3
        is_valid_places(places_nearby)
        for place in places_nearby:
            assert place["embedded_type"] == "stop_area"

    def test_places_nearby_with_coords_current_datetime(self):
        """places_nearby with _current_datetime"""

        id = "8.9831195195e-05;0.000898311281954"
        response = self.query_region("coords/{}/places_nearby?_current_datetime=20120815T160000".format(id))

        assert len(response['places_nearby']) > 0
        is_valid_places(response['places_nearby'])

        assert len(response['disruptions']) == 2
        disruptions = get_not_null(response, 'disruptions')
        disrupt = get_disruption(disruptions, 'too_bad')
        assert disrupt['disruption_id'] == 'disruption_on_stop_A'
        messages = get_not_null(disruptions[0], 'messages')

        assert (messages[0]['text']) == 'no luck'
        assert (messages[1]['text']) == 'try again'

    def test_wrong_places_nearby(self):
        """test that a wrongly formated query do not work on places_neaby"""

        lon = 10.0 / 111319.9
        lat = 100.0 / 111319.9
        response, status = self.query_region("bob/{};{}/places_nearby".format(lon, lat), check=False)

        assert status == 404
        # Note: it's not a canonical Navitia error with an Id and a message, but it don't seems to be
        # possible to do this with 404 (handled by flask)
        assert get_not_null(response, 'message')

        # same with a line (it has no meaning)
        response, status = self.query_region("lines/A/places_nearby".format(lon, lat), check=False)

        assert status == 404
        assert get_not_null(response, 'message')

    def test_non_existent_places_nearby(self):
        """test that a non existing place URI"""
        place_id = "I_am_not_existent"
        response, status = self.query_region("places/{}/places_nearby".format(place_id), check=False)

        assert response["error"]["message"] == "The entry point: {} is not valid".format(place_id)

    def test_non_existent_addresse(self):
        """test that a non existent addresse"""
        place_id = "-1.5348252000000002;47.2554241"
        response, status = self.query_region("places/{}".format(place_id), check=False)
        assert response["error"]["message"] == u'Unable to find place: -1.5348252000000002;47.2554241'

    def test_line_forbidden(self):
        """ test that line is not an allowed type """
        response, status = self.query_region("places?q=A&type[]=line", check=False)

        assert status == 400
        assert 'parameter "type[]" invalid' in response["message"]
        assert 'you gave line' in response["message"]

    def test_places_nearby_with_disable_disruption(self):
        """places_nearby with disable_disruption"""

        id = "8.9831195195e-05;0.000898311281954"
        response = self.query_region("coords/{}/places_nearby?_current_datetime=20120815T160000".format(id))
        assert len(response['places_nearby']) > 0
        is_valid_places(response['places_nearby'])
        assert len(response['disruptions']) == 2

        response = self.query_region(
            "coords/{}/places_nearby?_current_datetime=20120815T160000" "&disable_disruption=true".format(id)
        )
        assert len(response['places_nearby']) > 0
        is_valid_places(response['places_nearby'])
        assert len(response['disruptions']) == 0

        response = self.query_region(
            "coords/{}/places_nearby?_current_datetime=20120815T160000" "&disable_disruption=false".format(id)
        )
        assert len(response['places_nearby']) > 0
        is_valid_places(response['places_nearby'])
        assert len(response['disruptions']) == 2

    def test_main_stop_area_weight_factor(self):
        response = self.query_region("places?type[]=stop_area&q=stop")
        places = response['places']
        assert len(places) == 3
        assert places[0]['id'] == 'stopA'
        assert places[1]['id'] == 'stopB'
        assert places[2]['id'] == 'stopC'

        response = self.query_region("places?type[]=stop_area&q=stop&_main_stop_area_weight_factor=5")
        places = response['places']
        assert len(places) == 3
        assert places[0]['id'] == 'stopC'
        assert places[1]['id'] == 'stopA'
        assert places[2]['id'] == 'stopB'

    def test_disruptions_in_places(self):
        """check disruptions in places"""

        response = self.query_region("places?type[]=stop_area&q=stopA&_current_datetime=20120815T160000")
        places = response['places']
        assert len(places) == 1
        is_valid_places(places)
        assert len(response['disruptions']) == 0

        response = self.query_region(
            "places?type[]=stop_area&q=stopA&_current_datetime=20120815T160000" "&disable_disruption=true"
        )
        places = response['places']
        assert len(places) == 1
        is_valid_places(places)
        assert len(response['disruptions']) == 0

    def test_places_has_no_distance(self):
        """So far, Kraken has no distance returned from its API (as opposed to Bragi)
        We want to make sure that the distance field isn't returned in the response (as it will be zeroed)"
        """

        response = self.query_region("places?q=rue ab")
        places = response['places']
        assert len(places) == 1
        assert 'distance' not in places[0]
