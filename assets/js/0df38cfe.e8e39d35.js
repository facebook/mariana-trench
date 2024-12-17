"use strict";(self.webpackChunkwebsite=self.webpackChunkwebsite||[]).push([[569],{3905:(e,t,n)=>{n.r(t),n.d(t,{MDXContext:()=>s,MDXProvider:()=>p,mdx:()=>b,useMDXComponents:()=>m,withMDXComponents:()=>c});var r=n(67294);function i(e,t,n){return t in e?Object.defineProperty(e,t,{value:n,enumerable:!0,configurable:!0,writable:!0}):e[t]=n,e}function l(){return l=Object.assign||function(e){for(var t=1;t<arguments.length;t++){var n=arguments[t];for(var r in n)Object.prototype.hasOwnProperty.call(n,r)&&(e[r]=n[r])}return e},l.apply(this,arguments)}function a(e,t){var n=Object.keys(e);if(Object.getOwnPropertySymbols){var r=Object.getOwnPropertySymbols(e);t&&(r=r.filter((function(t){return Object.getOwnPropertyDescriptor(e,t).enumerable}))),n.push.apply(n,r)}return n}function o(e){for(var t=1;t<arguments.length;t++){var n=null!=arguments[t]?arguments[t]:{};t%2?a(Object(n),!0).forEach((function(t){i(e,t,n[t])})):Object.getOwnPropertyDescriptors?Object.defineProperties(e,Object.getOwnPropertyDescriptors(n)):a(Object(n)).forEach((function(t){Object.defineProperty(e,t,Object.getOwnPropertyDescriptor(n,t))}))}return e}function u(e,t){if(null==e)return{};var n,r,i=function(e,t){if(null==e)return{};var n,r,i={},l=Object.keys(e);for(r=0;r<l.length;r++)n=l[r],t.indexOf(n)>=0||(i[n]=e[n]);return i}(e,t);if(Object.getOwnPropertySymbols){var l=Object.getOwnPropertySymbols(e);for(r=0;r<l.length;r++)n=l[r],t.indexOf(n)>=0||Object.prototype.propertyIsEnumerable.call(e,n)&&(i[n]=e[n])}return i}var s=r.createContext({}),c=function(e){return function(t){var n=m(t.components);return r.createElement(e,l({},t,{components:n}))}},m=function(e){var t=r.useContext(s),n=t;return e&&(n="function"==typeof e?e(t):o(o({},t),e)),n},p=function(e){var t=m(e.components);return r.createElement(s.Provider,{value:t},e.children)},d={inlineCode:"code",wrapper:function(e){var t=e.children;return r.createElement(r.Fragment,{},t)}},f=r.forwardRef((function(e,t){var n=e.components,i=e.mdxType,l=e.originalType,a=e.parentName,s=u(e,["components","mdxType","originalType","parentName"]),c=m(n),p=i,f=c["".concat(a,".").concat(p)]||c[p]||d[p]||l;return n?r.createElement(f,o(o({ref:t},s),{},{components:n})):r.createElement(f,o({ref:t},s))}));function b(e,t){var n=arguments,i=t&&t.mdxType;if("string"==typeof e||i){var l=n.length,a=new Array(l);a[0]=f;var o={};for(var u in t)hasOwnProperty.call(t,u)&&(o[u]=t[u]);o.originalType=e,o.mdxType="string"==typeof e?e:i,a[1]=o;for(var s=2;s<l;s++)a[s]=n[s];return r.createElement.apply(null,a)}return r.createElement.apply(null,n)}f.displayName="MDXCreateElement"},84720:(e,t,n)=>{n.r(t),n.d(t,{assets:()=>u,contentTitle:()=>a,default:()=>m,frontMatter:()=>l,metadata:()=>o,toc:()=>s});var r=n(87462),i=(n(67294),n(3905));const l={id:"multi-source-sink-rules",title:"Multi-Source, Multi-Sink Rules",sidebar_label:"Multi-Source, Multi-Sink Rules"},a=void 0,o={unversionedId:"multi-source-sink-rules",id:"multi-source-sink-rules",title:"Multi-Source, Multi-Sink Rules",description:"Multi-Source, Multi-Sink Rules",source:"@site/documentation/multi_source_sink_rules.md",sourceDirName:".",slug:"/multi-source-sink-rules",permalink:"/docs/multi-source-sink-rules",draft:!1,editUrl:"https://github.com/facebook/mariana-trench/tree/main/documentation/website/documentation/multi_source_sink_rules.md",tags:[],version:"current",frontMatter:{id:"multi-source-sink-rules",title:"Multi-Source, Multi-Sink Rules",sidebar_label:"Multi-Source, Multi-Sink Rules"}},u={},s=[{value:"Multi-Source, Multi-Sink Rules",id:"multi-source-multi-sink-rules",level:3}],c={toc:s};function m(e){let{components:t,...n}=e;return(0,i.mdx)("wrapper",(0,r.Z)({},c,n,{components:t,mdxType:"MDXLayout"}),(0,i.mdx)("h3",{id:"multi-source-multi-sink-rules"},"Multi-Source, Multi-Sink Rules"),(0,i.mdx)("p",null,'Multi-source multi-sink rules are used to track the flow of taint from multiple sources to multiple sinks. This can, for example, be useful if you want to track both the source types "SensitiveData" and "WorldReadableFileLocation" to an IO operation as displayed in the code below.'),(0,i.mdx)("pre",null,(0,i.mdx)("code",{parentName:"pre",className:"language-java"},'File externalDir = context.getExternalFilesDir() // source WorldReadableFileLocation\nString sensitiveData = getUserToken() // source SensitiveData\n\nFile outputFile = new File(externalDir, "file.txt");\ntry (FileOutputStream fos = new FileOutputStream(outputFile)) {\n  fos.write(sensitiveData.getBytes()); // sink Argument(0) and Argument(1)\n}\n')),(0,i.mdx)("p",null,"Such a rule can be defined as follows:"),(0,i.mdx)("ol",null,(0,i.mdx)("li",{parentName:"ol"},"Define the sources as usual (see documentation above)."),(0,i.mdx)("li",{parentName:"ol"},"Define sinks on ",(0,i.mdx)("inlineCode",{parentName:"li"},"FileOutputStream::write")," as follows:")),(0,i.mdx)("pre",null,(0,i.mdx)("code",{parentName:"pre",className:"language-json"},'{\n  "model_generators": [\n    {\n      "find": "methods",\n      "where": [ /* name = write */ ... ],\n      "model": {\n        "sink": [\n          {\n            "kind": "PartialExternalFileWrite",\n            "partial_label": "outputStream",\n            "port": "Argument(0)"\n          },\n          {\n            "kind": "PartialExternalFileWrite",\n            "partial_label": "outputBytes",\n            "port": "Argument(1)"\n          }\n        ]\n      }\n    }\n}\n')),(0,i.mdx)("p",null,"There are ",(0,i.mdx)("strong",{parentName:"p"},"two")," sinks. There should always be as many sinks as there are sources, and the sinks must share the ",(0,i.mdx)("strong",{parentName:"p"},"same kind"),". This looks almost like a regular sink definition, with an additional ",(0,i.mdx)("strong",{parentName:"p"},"partial_label")," field. The ",(0,i.mdx)("inlineCode",{parentName:"p"},"partial_label")," field is used when defining the multi-source/sink rule below."),(0,i.mdx)("ol",{start:3},(0,i.mdx)("li",{parentName:"ol"},"Define rules as follows:")),(0,i.mdx)("pre",null,(0,i.mdx)("code",{parentName:"pre",className:"language-json"},'  {\n    "name": "Experimental: Some name",\n    "code": 9001,\n    "description": "More description here.",\n    "multi_sources": {\n      "outputBytes": [\n        "SensitiveData"\n      ],\n      "outputStream": [\n        "WorldReadableFileLocation"\n      ]\n    },\n    "partial_sinks": [\n      "PartialExternalFileWrite"\n    ]\n}\n')),(0,i.mdx)("p",null,"Pay attention to how the labels and partial sink kinds match what is defined in the sinks above."),(0,i.mdx)("blockquote",null,(0,i.mdx)("p",{parentName:"blockquote"},(0,i.mdx)("strong",{parentName:"p"},"NOTE:")," Multi-source/sink rules currently support exactly 2 sources/sinks only.")))}m.isMDXComponent=!0}}]);