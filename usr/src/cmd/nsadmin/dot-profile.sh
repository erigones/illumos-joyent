#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

#
# Copyright (c) 2019, Joyent, Inc.
# Copyright (c) 2019, Erigones, s. r. o.
#

ERIGONES_HOME="/opt/erigones"
VIRTUAL_ENV="$ERIGONES_HOME/envs"
PATH="/usr/bin:/usr/sbin:$ERIGONES_HOME/bin:$VIRTUAL_ENV/bin:/smartdc/bin:/opt/smartdc/bin:/opt/local/bin:/opt/local/sbin:/opt/tools/sbin:/opt/smartdc/agents/bin"
MANPATH="/usr/share/man:/smartdc/man:/opt/smartdc/man:/opt/local/man:/opt/tools/man"
PYTHONPATH="$ERIGONES_HOME:$PYTHONPATH"
PAGER=less
export PATH MANPATH PAGER ERIGONES_HOME PYTHONPATH VIRTUAL_ENV

# If pkgsrc-tools is set up and the mozilla-rootcerts package is installed
# configure the platform curl to use it.
if [[ -f /opt/tools/share/mozilla-rootcerts/cacert.pem ]]; then
	CURL_CA_BUNDLE=/opt/tools/share/mozilla-rootcerts/cacert.pem
fi
export CURL_CA_BUNDLE
