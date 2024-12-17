"use strict";(self.webpackChunkwebsite=self.webpackChunkwebsite||[]).push([[959,569,435],{3905:(e,t,n)=>{n.r(t),n.d(t,{MDXContext:()=>u,MDXProvider:()=>m,mdx:()=>x,useMDXComponents:()=>c,withMDXComponents:()=>d});var r=n(67294);function i(e,t,n){return t in e?Object.defineProperty(e,t,{value:n,enumerable:!0,configurable:!0,writable:!0}):e[t]=n,e}function o(){return o=Object.assign||function(e){for(var t=1;t<arguments.length;t++){var n=arguments[t];for(var r in n)Object.prototype.hasOwnProperty.call(n,r)&&(e[r]=n[r])}return e},o.apply(this,arguments)}function a(e,t){var n=Object.keys(e);if(Object.getOwnPropertySymbols){var r=Object.getOwnPropertySymbols(e);t&&(r=r.filter((function(t){return Object.getOwnPropertyDescriptor(e,t).enumerable}))),n.push.apply(n,r)}return n}function l(e){for(var t=1;t<arguments.length;t++){var n=null!=arguments[t]?arguments[t]:{};t%2?a(Object(n),!0).forEach((function(t){i(e,t,n[t])})):Object.getOwnPropertyDescriptors?Object.defineProperties(e,Object.getOwnPropertyDescriptors(n)):a(Object(n)).forEach((function(t){Object.defineProperty(e,t,Object.getOwnPropertyDescriptor(n,t))}))}return e}function s(e,t){if(null==e)return{};var n,r,i=function(e,t){if(null==e)return{};var n,r,i={},o=Object.keys(e);for(r=0;r<o.length;r++)n=o[r],t.indexOf(n)>=0||(i[n]=e[n]);return i}(e,t);if(Object.getOwnPropertySymbols){var o=Object.getOwnPropertySymbols(e);for(r=0;r<o.length;r++)n=o[r],t.indexOf(n)>=0||Object.prototype.propertyIsEnumerable.call(e,n)&&(i[n]=e[n])}return i}var u=r.createContext({}),d=function(e){return function(t){var n=c(t.components);return r.createElement(e,o({},t,{components:n}))}},c=function(e){var t=r.useContext(u),n=t;return e&&(n="function"==typeof e?e(t):l(l({},t),e)),n},m=function(e){var t=c(e.components);return r.createElement(u.Provider,{value:t},e.children)},p={inlineCode:"code",wrapper:function(e){var t=e.children;return r.createElement(r.Fragment,{},t)}},f=r.forwardRef((function(e,t){var n=e.components,i=e.mdxType,o=e.originalType,a=e.parentName,u=s(e,["components","mdxType","originalType","parentName"]),d=c(n),m=i,f=d["".concat(a,".").concat(m)]||d[m]||p[m]||o;return n?r.createElement(f,l(l({ref:t},u),{},{components:n})):r.createElement(f,l({ref:t},u))}));function x(e,t){var n=arguments,i=t&&t.mdxType;if("string"==typeof e||i){var o=n.length,a=new Array(o);a[0]=f;var l={};for(var s in t)hasOwnProperty.call(t,s)&&(l[s]=t[s]);l.originalType=e,l.mdxType="string"==typeof e?e:i,a[1]=l;for(var u=2;u<o;u++)a[u]=n[u];return r.createElement.apply(null,a)}return r.createElement.apply(null,n)}f.displayName="MDXCreateElement"},47441:(e,t,n)=>{n.r(t),n.d(t,{assets:()=>s,contentTitle:()=>a,default:()=>c,frontMatter:()=>o,metadata:()=>l,toc:()=>u});var r=n(87462),i=(n(67294),n(3905));const o={id:"exploitability-rules",title:"Exploitability Rules",sidebar_label:"Exploitability Rules"},a=void 0,l={unversionedId:"exploitability-rules",id:"exploitability-rules",title:"Exploitability Rules",description:"Exploitability Rules",source:"@site/documentation/exploitability_rules.md",sourceDirName:".",slug:"/exploitability-rules",permalink:"/docs/exploitability-rules",draft:!1,editUrl:"https://github.com/facebook/mariana-trench/tree/main/documentation/website/documentation/exploitability_rules.md",tags:[],version:"current",frontMatter:{id:"exploitability-rules",title:"Exploitability Rules",sidebar_label:"Exploitability Rules"}},s={},u=[{value:"Exploitability Rules",id:"exploitability-rules",level:3}],d={toc:u};function c(e){let{components:t,...n}=e;return(0,i.mdx)("wrapper",(0,r.Z)({},d,n,{components:t,mdxType:"MDXLayout"}),(0,i.mdx)("h3",{id:"exploitability-rules"},"Exploitability Rules"),(0,i.mdx)("p",null,"For android, some source to sink flows are only considered valid if the root callable of the source to sink flow is also accessible from outside the app.\nThis access is controlled by exported setting in the manifest file.\nExploitability rules allow us to additionally constraint source to sink rules on a call-chain flow from the root callable of the source to sink flow up to a call-chain effect source which is identified using the android manifest.\nEg."),(0,i.mdx)("pre",null,(0,i.mdx)("code",{parentName:"pre",className:"language-java"},"class ExportedActivity extends Activity {\n    void onCreate() {\n        Util.rootCallable(this);\n    }\n}\n\nclass Util {\n    void rootCallable(Activity activity) {\n        toSink(activity.getSource());\n    }\n}\n")),(0,i.mdx)("p",null,"Here, if we want to report an issue only if the android manifest sets ",(0,i.mdx)("inlineCode",{parentName:"p"},"exported: true")," for ",(0,i.mdx)("inlineCode",{parentName:"p"},"ExportedActivity"),". You can specify the explotability rule as follows:"),(0,i.mdx)("pre",null,(0,i.mdx)("code",{parentName:"pre",className:"language-json"},'{\n  "name": "Source to sink flow is reachable from an exported class",\n  "code": 1,\n  "description": "Values from source may eventually flow into sink",\n  "sources": [\n    "ActivityUserInput"\n  ],\n  "effect_sources": [\n    "Exported"\n  ],\n  "sinks": [\n    "LaunchingComponent"\n  ]\n}\n')),(0,i.mdx)("p",null,"Here, source to sink flow is found in ",(0,i.mdx)("inlineCode",{parentName:"p"},"rootCallable()")," but the issue will be reported iff ",(0,i.mdx)("inlineCode",{parentName:"p"},"ExportedActivity")," has ",(0,i.mdx)("inlineCode",{parentName:"p"},"exported:true")," in the android manifest."),(0,i.mdx)("pre",null,(0,i.mdx)("code",{parentName:"pre"},"                   ExportedActivity::onCreate(): with effect_source: Exported\n                           |\n                   rootCallable(): with inferred effect_sink: ActivityUserInput@LaunchingComponent\n                           |\n         +-----------------+-----------------+\n         |                                   |\n  getSource(): with                      toSink(): with\nsource kind: ActivityUserInput        sink kind: LaunchingComponent\n")))}c.isMDXComponent=!0},84720:(e,t,n)=>{n.r(t),n.d(t,{assets:()=>s,contentTitle:()=>a,default:()=>c,frontMatter:()=>o,metadata:()=>l,toc:()=>u});var r=n(87462),i=(n(67294),n(3905));const o={id:"multi-source-sink-rules",title:"Multi-Source, Multi-Sink Rules",sidebar_label:"Multi-Source, Multi-Sink Rules"},a=void 0,l={unversionedId:"multi-source-sink-rules",id:"multi-source-sink-rules",title:"Multi-Source, Multi-Sink Rules",description:"Multi-Source, Multi-Sink Rules",source:"@site/documentation/multi_source_sink_rules.md",sourceDirName:".",slug:"/multi-source-sink-rules",permalink:"/docs/multi-source-sink-rules",draft:!1,editUrl:"https://github.com/facebook/mariana-trench/tree/main/documentation/website/documentation/multi_source_sink_rules.md",tags:[],version:"current",frontMatter:{id:"multi-source-sink-rules",title:"Multi-Source, Multi-Sink Rules",sidebar_label:"Multi-Source, Multi-Sink Rules"}},s={},u=[{value:"Multi-Source, Multi-Sink Rules",id:"multi-source-multi-sink-rules",level:3}],d={toc:u};function c(e){let{components:t,...n}=e;return(0,i.mdx)("wrapper",(0,r.Z)({},d,n,{components:t,mdxType:"MDXLayout"}),(0,i.mdx)("h3",{id:"multi-source-multi-sink-rules"},"Multi-Source, Multi-Sink Rules"),(0,i.mdx)("p",null,'Multi-source multi-sink rules are used to track the flow of taint from multiple sources to multiple sinks. This can, for example, be useful if you want to track both the source types "SensitiveData" and "WorldReadableFileLocation" to an IO operation as displayed in the code below.'),(0,i.mdx)("pre",null,(0,i.mdx)("code",{parentName:"pre",className:"language-java"},'File externalDir = context.getExternalFilesDir() // source WorldReadableFileLocation\nString sensitiveData = getUserToken() // source SensitiveData\n\nFile outputFile = new File(externalDir, "file.txt");\ntry (FileOutputStream fos = new FileOutputStream(outputFile)) {\n  fos.write(sensitiveData.getBytes()); // sink Argument(0) and Argument(1)\n}\n')),(0,i.mdx)("p",null,"Such a rule can be defined as follows:"),(0,i.mdx)("ol",null,(0,i.mdx)("li",{parentName:"ol"},"Define the sources as usual (see documentation above)."),(0,i.mdx)("li",{parentName:"ol"},"Define sinks on ",(0,i.mdx)("inlineCode",{parentName:"li"},"FileOutputStream::write")," as follows:")),(0,i.mdx)("pre",null,(0,i.mdx)("code",{parentName:"pre",className:"language-json"},'{\n  "model_generators": [\n    {\n      "find": "methods",\n      "where": [ /* name = write */ ... ],\n      "model": {\n        "sink": [\n          {\n            "kind": "PartialExternalFileWrite",\n            "partial_label": "outputStream",\n            "port": "Argument(0)"\n          },\n          {\n            "kind": "PartialExternalFileWrite",\n            "partial_label": "outputBytes",\n            "port": "Argument(1)"\n          }\n        ]\n      }\n    }\n}\n')),(0,i.mdx)("p",null,"There are ",(0,i.mdx)("strong",{parentName:"p"},"two")," sinks. There should always be as many sinks as there are sources, and the sinks must share the ",(0,i.mdx)("strong",{parentName:"p"},"same kind"),". This looks almost like a regular sink definition, with an additional ",(0,i.mdx)("strong",{parentName:"p"},"partial_label")," field. The ",(0,i.mdx)("inlineCode",{parentName:"p"},"partial_label")," field is used when defining the multi-source/sink rule below."),(0,i.mdx)("ol",{start:3},(0,i.mdx)("li",{parentName:"ol"},"Define rules as follows:")),(0,i.mdx)("pre",null,(0,i.mdx)("code",{parentName:"pre",className:"language-json"},'  {\n    "name": "Experimental: Some name",\n    "code": 9001,\n    "description": "More description here.",\n    "multi_sources": {\n      "outputBytes": [\n        "SensitiveData"\n      ],\n      "outputStream": [\n        "WorldReadableFileLocation"\n      ]\n    },\n    "partial_sinks": [\n      "PartialExternalFileWrite"\n    ]\n}\n')),(0,i.mdx)("p",null,"Pay attention to how the labels and partial sink kinds match what is defined in the sinks above."),(0,i.mdx)("blockquote",null,(0,i.mdx)("p",{parentName:"blockquote"},(0,i.mdx)("strong",{parentName:"p"},"NOTE:")," Multi-source/sink rules currently support exactly 2 sources/sinks only.")))}c.isMDXComponent=!0},65133:(e,t,n)=>{n.r(t),n.d(t,{assets:()=>d,contentTitle:()=>s,default:()=>p,frontMatter:()=>l,metadata:()=>u,toc:()=>c});var r=n(87462),i=(n(67294),n(3905)),o=n(84720),a=n(47441);const l={id:"rules",title:"Rules",sidebar_label:"Rules"},s=void 0,u={unversionedId:"rules",id:"rules",title:"Rules",description:"A rule describes flows that we want to catch (e.g, user input flowing into command execution).",source:"@site/documentation/rules.md",sourceDirName:".",slug:"/rules",permalink:"/docs/rules",draft:!1,editUrl:"https://github.com/facebook/mariana-trench/tree/main/documentation/website/documentation/rules.md",tags:[],version:"current",frontMatter:{id:"rules",title:"Rules",sidebar_label:"Rules"},sidebar:"docs",previous:{title:"Customize Sources and Sinks",permalink:"/docs/customize-sources-and-sinks"},next:{title:"Models & Model Generators",permalink:"/docs/models"}},d={},c=[{value:"Transform Rules",id:"transform-rules",level:3}],m={toc:c};function p(e){let{components:t,...n}=e;return(0,i.mdx)("wrapper",(0,r.Z)({},m,n,{components:t,mdxType:"MDXLayout"}),(0,i.mdx)("p",null,"A rule describes flows that we want to catch (e.g, user input flowing into command execution).\nA rule is made of a set of source ",(0,i.mdx)("a",{parentName:"p",href:"/docs/models#kinds"},"kinds"),", a set of sink kinds, a name, a code, and a description."),(0,i.mdx)("p",null,"Here is an example of a rule in JSON:"),(0,i.mdx)("pre",null,(0,i.mdx)("code",{parentName:"pre",className:"language-json"},'{\n  "name": "User input flows into code execution (RCE)",\n  "code": 1,\n  "description": "Values from user-controlled source may eventually flow into code execution",\n  "sources": [\n    "UserCamera",\n    "UserInput",\n  ],\n  "sinks": [\n    "CodeAsyncJob",\n    "CodeExecution",\n  ]\n}\n')),(0,i.mdx)("p",null,"For guidance on modeling sources and sinks, see the next section, ",(0,i.mdx)("a",{parentName:"p",href:"/docs/models"},"Models and Model Generators"),"."),(0,i.mdx)("p",null,"Rules used by Mariana Trench can be specified with the ",(0,i.mdx)("inlineCode",{parentName:"p"},"--rules-paths")," argument. The default set of rules that run can be found in ",(0,i.mdx)("a",{parentName:"p",href:"https://github.com/facebook/mariana-trench/blob/main/configuration/rules.json"},"configuration/rules.json"),"."),(0,i.mdx)("h3",{id:"transform-rules"},"Transform Rules"),(0,i.mdx)("p",null,"Some flows are only interesting if they also pass through a specific method. These methods can be modeled as a propagation with transforms. Then, to catch these flows, we specify the ordered list of transforms here in the rule."),(0,i.mdx)("p",null,"Here is an example of a transform rule in JSON:"),(0,i.mdx)("pre",null,(0,i.mdx)("code",{parentName:"pre",className:"language-json"},'{\n  "name": "URI Query Parameters flow into Internal Intent data",\n  "code": 2,\n  "oncall": "prodsec_mobile",\n  "description": "Values from a query parameter source may eventually flow into Internal Intent data",\n  "sources": [\n    "UriQueryParameter"\n  ],\n  "transforms": [\n    "IntentData"\n  ],\n  "sinks": [\n    "LaunchingFamilyComponent"\n  ]\n}\n')),(0,i.mdx)("p",null,"The flow will only be created if UriQueryParameter flows through IntentData and then into LaunchingFamilyComponent. It will not be created when UriQueryParameter flows into LaunchingFamilyComponent without passing through the IntentData transform."),(0,i.mdx)("p",null,"See ",(0,i.mdx)("a",{parentName:"p",href:"/docs/models#propagation-with-transforms"},"Models and Model Generators")," for how to model transforms."),(0,i.mdx)(o.default,{mdxType:"MultiSourceSinkRule"}),(0,i.mdx)(a.default,{mdxType:"ExploitabilityRules"}))}p.isMDXComponent=!0}}]);