/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @format
 */

const {fbContent, fbInternalOnly} = require('internaldocs-fb-helpers');

module.exports = {
  docs: {
  "User Guide": [
    'overview',
    'getting-started',
    fbInternalOnly('fb/running-on-apps'),
    fbInternalOnly('fb/running-on-3rd-party-apps'),
    'customize_sources_and_sinks',
    'models',
    'configuration'],
  "Developer Guide": [
    fbContent({external: 'contribution'}),
    fbInternalOnly('fb/running-tests'),
    fbInternalOnly('fb/inspecting-an-apk'),
    fbInternalOnly('fb/android-lifecycles'),
    fbInternalOnly('fb/debugging-fp-fns'),
    fbInternalOnly('fb/deployment'),
    fbInternalOnly('fb/continuous-and-diff-runs'),
    fbInternalOnly("fb/crtex"),
    ]
  },
};
