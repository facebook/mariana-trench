/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @format
 */

const {
  fbContent,
  fbInternalOnly,
} = require('docusaurus-plugin-internaldocs-fb/internal');

module.exports = {
  docs: {
    'User Guide': [
      'overview',
      fbContent({external: 'getting-started'}),
      fbInternalOnly('fb/xfn-onboarding'),
      fbInternalOnly('fb/using-mt-101'),
      fbInternalOnly('fb/running-on-apps'),
      fbInternalOnly('fb/running-on-new-apps'),
      fbInternalOnly('fb/running-on-3rd-party-apps'),
      'configuration',
      'customize-sources-and-sinks',
      'rules',
      'models',
      'shims',
      'feature-descriptions',
      'known-false-negatives',
      fbInternalOnly('fb/testing-diffs'),
    ],
    'Developer Guide': [
      fbContent({external: 'build-from-source'}),
      fbInternalOnly('fb/developer-getting-started'),
      fbInternalOnly('fb/running-tests'),
      fbInternalOnly('fb/inspecting-an-apk'),
      fbInternalOnly('fb/android-lifecycles'),
      'debugging-fp-fns',
      fbInternalOnly('fb/deployment'),
      fbInternalOnly('fb/continuous-and-diff-runs'),
      fbInternalOnly('fb/crtex'),
      fbInternalOnly('fb/alerts'),
    ],
  },
};
