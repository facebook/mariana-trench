"use strict";(self.webpackChunkwebsite=self.webpackChunkwebsite||[]).push([[959],{3905:(e,n,t)=>{t.r(n),t.d(n,{MDXContext:()=>l,MDXProvider:()=>p,mdx:()=>b,useMDXComponents:()=>d,withMDXComponents:()=>u});var r=t(67294);function o(e,n,t){return n in e?Object.defineProperty(e,n,{value:t,enumerable:!0,configurable:!0,writable:!0}):e[n]=t,e}function a(){return a=Object.assign||function(e){for(var n=1;n<arguments.length;n++){var t=arguments[n];for(var r in t)Object.prototype.hasOwnProperty.call(t,r)&&(e[r]=t[r])}return e},a.apply(this,arguments)}function i(e,n){var t=Object.keys(e);if(Object.getOwnPropertySymbols){var r=Object.getOwnPropertySymbols(e);n&&(r=r.filter((function(n){return Object.getOwnPropertyDescriptor(e,n).enumerable}))),t.push.apply(t,r)}return t}function s(e){for(var n=1;n<arguments.length;n++){var t=null!=arguments[n]?arguments[n]:{};n%2?i(Object(t),!0).forEach((function(n){o(e,n,t[n])})):Object.getOwnPropertyDescriptors?Object.defineProperties(e,Object.getOwnPropertyDescriptors(t)):i(Object(t)).forEach((function(n){Object.defineProperty(e,n,Object.getOwnPropertyDescriptor(t,n))}))}return e}function c(e,n){if(null==e)return{};var t,r,o=function(e,n){if(null==e)return{};var t,r,o={},a=Object.keys(e);for(r=0;r<a.length;r++)t=a[r],n.indexOf(t)>=0||(o[t]=e[t]);return o}(e,n);if(Object.getOwnPropertySymbols){var a=Object.getOwnPropertySymbols(e);for(r=0;r<a.length;r++)t=a[r],n.indexOf(t)>=0||Object.prototype.propertyIsEnumerable.call(e,t)&&(o[t]=e[t])}return o}var l=r.createContext({}),u=function(e){return function(n){var t=d(n.components);return r.createElement(e,a({},n,{components:t}))}},d=function(e){var n=r.useContext(l),t=n;return e&&(t="function"==typeof e?e(n):s(s({},n),e)),t},p=function(e){var n=d(e.components);return r.createElement(l.Provider,{value:n},e.children)},m={inlineCode:"code",wrapper:function(e){var n=e.children;return r.createElement(r.Fragment,{},n)}},f=r.forwardRef((function(e,n){var t=e.components,o=e.mdxType,a=e.originalType,i=e.parentName,l=c(e,["components","mdxType","originalType","parentName"]),u=d(t),p=o,f=u["".concat(i,".").concat(p)]||u[p]||m[p]||a;return t?r.createElement(f,s(s({ref:n},l),{},{components:t})):r.createElement(f,s({ref:n},l))}));function b(e,n){var t=arguments,o=n&&n.mdxType;if("string"==typeof e||o){var a=t.length,i=new Array(a);i[0]=f;var s={};for(var c in n)hasOwnProperty.call(n,c)&&(s[c]=n[c]);s.originalType=e,s.mdxType="string"==typeof e?e:o,i[1]=s;for(var l=2;l<a;l++)i[l]=t[l];return r.createElement.apply(null,i)}return r.createElement.apply(null,t)}f.displayName="MDXCreateElement"},65133:(e,n,t)=>{t.r(n),t.d(n,{assets:()=>c,contentTitle:()=>i,default:()=>m,frontMatter:()=>a,metadata:()=>s,toc:()=>l});var r=t(87462),o=(t(67294),t(3905));const a={id:"rules",title:"Rules",sidebar_label:"Rules"},i=void 0,s={unversionedId:"rules",id:"rules",title:"Rules",description:"A rule describes flows that we want to catch (e.g, user input flowing into command execution).",source:"@site/documentation/rules.md",sourceDirName:".",slug:"/rules",permalink:"/docs/rules",draft:!1,editUrl:"https://github.com/facebook/mariana-trench/tree/main/documentation/website/documentation/rules.md",tags:[],version:"current",frontMatter:{id:"rules",title:"Rules",sidebar_label:"Rules"},sidebar:"docs",previous:{title:"Customize Sources and Sinks",permalink:"/docs/customize-sources-and-sinks"},next:{title:"Models & Model Generators",permalink:"/docs/models"}},c={},l=[],u=(d="MultiSourceSinkRule",function(e){return console.warn("Component "+d+" was not imported, exported, or provided by MDXProvider as global scope"),(0,o.mdx)("div",e)});var d;const p={toc:l};function m(e){let{components:n,...t}=e;return(0,o.mdx)("wrapper",(0,r.Z)({},p,t,{components:n,mdxType:"MDXLayout"}),(0,o.mdx)("p",null,"A rule describes flows that we want to catch (e.g, user input flowing into command execution).\nA rule is made of a set of source ",(0,o.mdx)("a",{parentName:"p",href:"/docs/models#kinds"},"kinds"),", a set of sink kinds, a name, a code, and a description."),(0,o.mdx)("p",null,"Here is an example of a rule in JSON:"),(0,o.mdx)("pre",null,(0,o.mdx)("code",{parentName:"pre",className:"language-json"},'{\n  "name": "User input flows into code execution (RCE)",\n  "code": 1,\n  "description": "Values from user-controlled source may eventually flow into code execution",\n  "sources": [\n    "UserCamera",\n    "UserInput",\n  ],\n  "sinks": [\n    "CodeAsyncJob",\n    "CodeExecution",\n  ]\n}\n')),(0,o.mdx)("p",null,"For guidance on modeling sources and sinks, see the next section, ",(0,o.mdx)("a",{parentName:"p",href:"/docs/models"},"Models and Model Generators"),"."),(0,o.mdx)("p",null,"Rules used by Mariana Trench can be specified with the ",(0,o.mdx)("inlineCode",{parentName:"p"},"--rules-paths")," argument. The default set of rules that run can be found in ",(0,o.mdx)("a",{parentName:"p",href:"https://github.com/facebook/mariana-trench/blob/main/configuration/rules.json"},"configuration/rules.json"),"."),(0,o.mdx)(u,{mdxType:"MultiSourceSinkRule"}))}m.isMDXComponent=!0}}]);