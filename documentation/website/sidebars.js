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
  docs: [
    'overview',
    {
      type: 'category',
      label: 'Mariana Trench Concepts',
      collapsed: false,
      items: [
        fbInternalOnly('fb/xfn-onboarding'),
        fbInternalOnly('fb/using-mt-101'),
        'known-false-negatives',
      ],
    },
    {
      type: 'category',
      label: 'User Guide',
      collapsed: false,
      items: [
        fbContent({external: 'getting-started'}),
        fbInternalOnly({
          type: 'category',
          label: 'Running Mariana Trench',
          items: [
            'fb/running-on-apps',
            'fb/running-on-new-apps',
            'fb/running-on-3rd-party-apps',
          ],
        }),
        'configuration',
        fbInternalOnly('fb/app-configuration'),
        {
          type: 'category',
          label: 'Model/Taint Configuration',
          items: [
            'customize-sources-and-sinks',
            'rules',
            'models',
            fbInternalOnly('fb/kss-usage'),
          ],
        },
        {
          type: 'category',
          label: 'Call Graph Configuration',
          items: [
            'shims',
            'android-lifecycles',
          ],
        },
        'feature-descriptions',
        fbInternalOnly('fb/testing-diffs'),
      ]
    },
    {
      type: 'category',
      label: 'Developer Guide',
      collapsed: false,
      items: [
        fbContent({external: 'build-from-source'}),
        fbInternalOnly('fb/developer-getting-started'),
        fbInternalOnly('fb/running-tests'),
        fbInternalOnly('fb/inspecting-an-apk'),
        'debugging-fp-fns',
        fbInternalOnly('fb/deployment'),
        fbInternalOnly('fb/continuous-and-diff-runs'),
        fbInternalOnly('fb/crtex'),
        fbInternalOnly('fb/alerts'),
        fbInternalOnly('fb/marianabench'),
        fbInternalOnly('fb/kss-impl'),
      ],
    },
  ],
};
