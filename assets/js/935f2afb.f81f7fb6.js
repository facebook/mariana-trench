"use strict";(self.webpackChunkwebsite=self.webpackChunkwebsite||[]).push([[53],{1109:e=>{e.exports=JSON.parse('{"pluginId":"default","version":"current","label":"Next","banner":null,"badge":false,"noIndex":false,"className":"docs-version-current","isLast":true,"docsSidebars":{"docs":[{"type":"category","label":"User Guide","items":[{"type":"link","label":"Overview","href":"/docs/overview","docId":"overview"},{"type":"link","label":"Getting Started","href":"/docs/getting-started","docId":"getting-started"},{"type":"link","label":"Configuration","href":"/docs/configuration","docId":"configuration"},{"type":"link","label":"Customize Sources and Sinks","href":"/docs/customize-sources-and-sinks","docId":"customize-sources-and-sinks"},{"type":"link","label":"Rules","href":"/docs/rules","docId":"rules"},{"type":"link","label":"Models & Model Generators","href":"/docs/models","docId":"models"},{"type":"link","label":"Shims","href":"/docs/shims","docId":"shims"},{"type":"link","label":"Feature Glossary","href":"/docs/feature-descriptions","docId":"feature-descriptions"},{"type":"link","label":"Known False Negatives","href":"/docs/known-false-negatives","docId":"known-false-negatives"}],"collapsed":true,"collapsible":true},{"type":"category","label":"Developer Guide","items":[{"type":"link","label":"Contribution","href":"/docs/contribution","docId":"contribution"},{"type":"link","label":"Debugging False Positives/False Negatives","href":"/docs/debugging-fp-fns","docId":"debugging-fp-fns"}],"collapsed":true,"collapsible":true}]},"docs":{"configuration":{"id":"configuration","title":"Configuration","description":"Mariana Trench is highly configurable and we recommend that you invest time into adjusting the tool to your specific use cases. At Facebook, we have dedicated security engineers that will spend a significant amount of their time adding new rules and model generators to improve the analysis results.","sidebar":"docs"},"contribution":{"id":"contribution","title":"Contribution","description":"This documentation aims to help you become an active contributor to Mariana Trench.","sidebar":"docs"},"customize-sources-and-sinks":{"id":"customize-sources-and-sinks","title":"Customize Sources and Sinks","description":"Overview","sidebar":"docs"},"debugging-fp-fns":{"id":"debugging-fp-fns","title":"Debugging False Positives/False Negatives","description":"This document is mainly intended for software engineers, to help them debug false positives and false negatives.","sidebar":"docs"},"feature-descriptions":{"id":"feature-descriptions","title":"Feature Glossary","description":"As explained in the features section of the models wiki, a feature can be used to tag a flow and help filtering issues. A feature describes a property of a flow. A feature can be any arbitrary string. A feature that\'s prefixed with always- signals that every path in the issue has that feature associated with it, while lacking that prefix means that at least one path, but not all paths, contains that feature.","sidebar":"docs"},"getting-started":{"id":"getting-started","title":"Getting Started","description":"This guide will walk you through setting up Mariana Trench on your machine and get you to find your first remote code execution vulnerability in a small sample app.","sidebar":"docs"},"known-false-negatives":{"id":"known-false-negatives","title":"Known False Negatives","description":"Like any static analysis tools, Mariana Trench has false negatives. This documents the more well-known places where taint is dropped. Note that this is not an exhaustive list. See this wiki for instructions on how to debug them.","sidebar":"docs"},"models":{"id":"models","title":"Models & Model Generators","description":"The main way to configure the analysis is through defining model generators. Each model generator defines (1) a filter, made up of constraints to specify the methods (or fields) for which a model should be generated, and (2) a model, an abstract representation of how data flows through a method.","sidebar":"docs"},"overview":{"id":"overview","title":"Overview","description":"What is Mariana Trench","sidebar":"docs"},"rules":{"id":"rules","title":"Rules","description":"A rule describes flows that we want to catch (e.g, user input flowing into command execution).","sidebar":"docs"},"shims":{"id":"shims","title":"Shims","description":"What is a \\"shim\\"?","sidebar":"docs"}}}')}}]);