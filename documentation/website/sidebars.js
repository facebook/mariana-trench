/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @format
 */

const {fbInternalOnly} = require('internaldocs-fb-helpers');

module.exports = {
  someSidebar: [
    'getting-started',
    fbInternalOnly('fb/running-tests'),
    fbInternalOnly('fb/running-on-apps'),
    fbInternalOnly('fb/running-on-3rd-party-apps'),
    'configuration',
    'models',
    'developers-guide',
  ],
};
