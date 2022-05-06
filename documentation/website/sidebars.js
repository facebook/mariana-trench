/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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
    fbContent({external: 'getting-started'}),
    fbInternalOnly("fb/using-mt-101"),
    fbInternalOnly("fb/xfn-onboarding"),
    fbInternalOnly('fb/running-on-apps'),
    fbInternalOnly('fb/running-on-3rd-party-apps'),
    'customize-sources-and-sinks',
    'models',
    'configuration',
    fbInternalOnly("fb/known-false-negatives"),
    fbInternalOnly("fb/testing-diffs"),
    fbInternalOnly("fb/running-on-new-apps"),
],
  "Developer Guide": [
    fbContent({external: 'contribution'}),
    fbInternalOnly('fb/developer-getting-started'),
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
