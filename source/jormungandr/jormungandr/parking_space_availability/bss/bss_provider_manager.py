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
from jormungandr.parking_space_availability.abstract_provider_manager import AbstractProviderManager
import logging
import datetime

POI_TYPE_ID = 'poi_type:amenity:bicycle_rental'


class BssProviderManager(AbstractProviderManager):
    def __init__(self, bss_providers_configuration, providers_getter=None, update_interval=60):
        super(BssProviderManager, self).__init__()
        self._bss_providers_legacy = []
        self._bss_providers = {}
        self._bss_providers_last_update = {}
        self._last_update = datetime.datetime(1970, 1, 1)
        self._update_interval = update_interval
        self._providers_getter = providers_getter
        for configuration in bss_providers_configuration:
            arguments = configuration.get('args', {})
            self._bss_providers_legacy.append(self._init_class(configuration['class'], arguments))

    def update_config(self):
        if (
            self._last_update + datetime.timedelta(seconds=self._update_interval) > datetime.datetime.utcnow()
            or not self._providers_getter
        ):
            return

        logger = logging.getLogger(__name__)
        logger.debug('updating bss providers')
        self._last_update = datetime.datetime.utcnow()

        providers = None
        try:
            providers = self._providers_getter()
        except Exception:
            logger.exception('failure to retrieve bss configuration')
        if not providers:
            logger.debug('all providers have be disabled')
            self._bss_providers = {}
            self._bss_providers_last_update = {}
            return

        logger.debug('updating with %s', providers)
        for provider in providers:
            # it's a new bss provider or it has been updated, we add it
            if (
                provider.id not in self._bss_providers_last_update
                or provider.last_update() > self._bss_providers_last_update[provider.id]
            ):
                self.update_provider(provider)
        # remove deleted providers
        for to_delete in set(self._bss_providers.keys()) - {p.id for p in providers}:
            del self._bss_providers[to_delete]
            del self._bss_providers_last_update[to_delete]
            logger.info('deleting bss provider %s', to_delete)

    def update_provider(self, provider):
        logger = logging.getLogger(__name__)
        logger.info(
            'updating/adding %s bss provider with the following configuration: %s',
            provider.id,
            provider.full_args(),
        )
        try:
            self._bss_providers[provider.id] = self._init_class(provider.klass, provider.full_args())
            self._bss_providers_last_update[provider.id] = provider.last_update()
        except Exception:
            logger.exception('impossible to initialize bss provider')

    def _handle_poi(self, item):
        if 'poi_type' in item and item['poi_type']['id'] == POI_TYPE_ID:
            provider = self._find_provider(item)
            if provider:
                item['stands'] = provider.get_informations(item)
                return provider
        return None

    # TODO use public version everywhere
    def _get_providers(self):
        self.update_config()
        # providers from the database have priority on legacies providers
        return list(self._bss_providers.values()) + self._bss_providers_legacy

    def get_providers(self):
        return self._get_providers()
