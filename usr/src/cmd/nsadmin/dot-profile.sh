#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

#
# Copyright (c) 2019, Joyent, Inc.
# Copyright (c) 2019, Erigones, s. r. o.
# Copyright 2019 Joyent, Inc.
# Copyright 2024 MNX Cloud, Inc.
# Copyright 2024 Danube Cloud
#

ERIGONES_HOME="/opt/erigones"
VIRTUAL_ENV="$ERIGONES_HOME/envs"
PATH="/usr/bin:/usr/sbin:$ERIGONES_HOME/bin:$VIRTUAL_ENV/bin:/smartdc/bin:/opt/smartdc/bin:/opt/local/bin:/opt/local/sbin:/opt/tools/sbin:/opt/smartdc/agents/bin"
MANPATH="/usr/share/man:/smartdc/man:/opt/smartdc/man:/opt/local/man:/opt/tools/man"
PYTHONPATH="$ERIGONES_HOME:$PYTHONPATH"
PAGER=less
export PATH MANPATH PAGER ERIGONES_HOME PYTHONPATH VIRTUAL_ENV

# If pkgsrc-tools is set up and either the mozilla-rootcerts-openssl or
# mozilla-rootcerts packages are installed, configure the platform curl to
# use the provided CA bundle.
if [[ -f /opt/tools/share/mozilla-rootcerts/cacert.pem ]]; then
	CURL_CA_BUNDLE=/opt/tools/share/mozilla-rootcerts/cacert.pem
elif [[ -f /opt/tools/etc/openssl/certs/ca-certificates.crt ]]; then
	CURL_CA_BUNDLE=/opt/tools/etc/openssl/certs/ca-certificates.crt
fi
export CURL_CA_BUNDLE
