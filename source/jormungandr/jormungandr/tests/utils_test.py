# coding=utf-8
# Copyright (c) 2001-2016, Canal TP and/or its affiliates. All rights reserved.
#
# This file is part of Navitia,
# the software to build cool stuff with public transport.
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

from __future__ import absolute_import, unicode_literals
from contextlib import contextmanager
from flask import appcontext_pushed, g
from jormungandr.utils import timestamp_to_datetime, make_namedtuple, walk_dict
import pytz
from jormungandr import app
import datetime
import io

from navitiacommon import models
import pytest


class MockResponse(object):
    """
    small class to mock an http response
    """

    def __init__(self, data, status_code, url=None, *args, **kwargs):
        self.data = data
        self.status_code = status_code
        self.url = url

    def json(self):
        return self.data

    @property
    def text(self):
        return self.data

    @property
    def content(self):
        return self.data


class MockRequests(object):
    """
    small class to mock an http request
    """

    def __init__(self, responses):
        self.responses = responses

    def get(self, url, *args, **kwargs):
        params = kwargs.get('params')
        if params:
            from six.moves.urllib.parse import urlencode

            params.sort()
            url += "?{}".format(urlencode(params, doseq=True))

        r = self.responses.get(url)
        if r:
            return MockResponse(r[0], r[1], url)
        else:
            raise Exception("impossible to find mock response for url {}".format(url))

    def post(self, *args, **kwargs):
        return self.get(*args, **kwargs)


class FakeUser(models.User):
    """
    We create a user independent from a database
    """

    def __init__(
        self,
        name,
        id,
        have_access_to_free_instances=True,
        is_super_user=False,
        is_blocked=False,
        shape=None,
        default_coord=None,
    ):
        """
        We just need a fake user, we don't really care about its identity
        """
        self.id = id
        self.login = name
        self.type = 'with_free_instances'
        if not have_access_to_free_instances:
            self.type = 'without_free_instances'
        if is_super_user:
            self.type = 'super_user'
        self.end_point_id = None
        self._is_blocked = is_blocked
        self.shape = shape
        self.default_coord = default_coord

    @classmethod
    def get_from_token(cls, token, valid_until):
        """
        Create an empty user
        Must be overridden
        """
        assert False

    def is_blocked(self, datetime_utc):
        """
        Return True if user is blocked else False
        """
        return self._is_blocked


@contextmanager
def user_set(app, fake_user_type, user_name):
    """
    set the test user doing the request
    """

    def handler(sender, **kwargs):
        g.user = fake_user_type.get_from_token(user_name, valid_until=None)

    with appcontext_pushed.connected_to(handler, app):
        yield


def test_timestamp_to_datetime():
    # timestamp > MAX_INT
    assert timestamp_to_datetime(18446744071562142720) is None

    with app.app_context():
        g.timezone = pytz.utc
        # test valid date
        assert timestamp_to_datetime(1493296245) == datetime.datetime(2017, 4, 27, 12, 30, 45, tzinfo=pytz.UTC)

        g.timezone = None
        # test valid date but no timezone
        assert timestamp_to_datetime(1493296245) is None


def test_make_named_tuple():
    Bob = make_namedtuple('Bob', 'a', 'b', c=2, d=14)
    b = Bob(b=14, a=12)
    assert b.a == 12
    assert b.b == 14
    assert b.c == 2
    assert b.d == 14
    b = Bob(14, 12)  # non named argument also works
    assert b.a == 14
    assert b.b == 12
    assert b.c == 2
    assert b.d == 14
    b = Bob(12, b=14, d=123)
    assert b.a == 12
    assert b.b == 14
    assert b.c == 2
    assert b.d == 123
    with pytest.raises(TypeError):
        Bob(a=12)
    with pytest.raises(TypeError):
        Bob()


def test_walk_dict():
    bob = {
        'tutu': 1,
        'tata': [1, 2],
        'toto': {'bob': 12, 'bobette': 13, 'nested_bob': {'bob': 3}},
        'tete': ('tuple1', ['ltuple1', 'ltuple2']),
        'titi': [{'a': 1}, {'b': 1}],
    }
    result = io.StringIO()

    def my_visitor(name, val):
        result.write("{}={}\n".format(name, val))

    walk_dict(bob, my_visitor)
    expected = """titi={u'b': 1}
b=1
titi={u'a': 1}
a=1
tete=ltuple2
tete=ltuple1
tete=tuple1
tutu=1
toto={u'bobette': 13, u'bob': 12, u'nested_bob': {u'bob': 3}}
nested_bob={u'bob': 3}
bob=3
bob=12
bobette=13
tata=2
tata=1
"""
    assert result.getvalue() == expected

    result = io.StringIO()

    def my_stoper_visitor(name, val):
        result.write("{}={}\n".format(name, val))
        if name == 'tete':
            return True

    walk_dict(bob, my_stoper_visitor)
    expected = """titi={u'b': 1}
b=1
titi={u'a': 1}
a=1
tete=ltuple2
"""
    assert result.getvalue() == expected
