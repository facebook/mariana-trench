"use strict";(self.webpackChunkwebsite=self.webpackChunkwebsite||[]).push([[195],{54338:function(e,t,a){a.r(t),a.d(t,{default:function(){return f}});var n=a(67294);function r(e){var t,a,n="";if("string"==typeof e||"number"==typeof e)n+=e;else if("object"==typeof e)if(Array.isArray(e))for(t=0;t<e.length;t++)e[t]&&(a=r(e[t]))&&(n&&(n+=" "),n+=a);else for(t in e)e[t]&&(n&&(n+=" "),n+=t);return n}function i(){for(var e,t,a=0,n="";a<arguments.length;)(e=arguments[a++])&&(t=r(e))&&(n&&(n+=" "),n+=t);return n}var l=a(87190),o=a(39960),c=a(52263),s=a(44996),u={heroBanner:"heroBanner_UJJx",buttons:"buttons_pzbO",features:"features_keug",featureImage:"featureImage_yA8i"},m=[{title:"Android and Java",description:n.createElement(n.Fragment,null,"Find security vulnerabilities in Android and Java applications. Mariana Trench analyzes Dalvik bytecode.")},{title:"Fast",description:n.createElement(n.Fragment,null,"Mariana Trench is built to run fast on large codebases (10s of millions of lines of code). It can find vulnerabilities as code changes, before it ever lands in your repository.")},{title:"Customizable",description:n.createElement(n.Fragment,null,"Adapt Mariana Trench to your needs. Find the vulnerabilities that you care about by teaching Mariana Trench where data comes from and where you do not want it to go.")}];function d(e){var t=e.imageUrl,a=e.title,r=e.description,l=(0,s.default)(t);return n.createElement("div",{className:i("col col--4",u.feature)},l&&n.createElement("div",{className:"text--center"},n.createElement("img",{className:u.featureImage,src:l,alt:a})),n.createElement("h3",null,a),n.createElement("p",null,r))}var f=function(){var e=(0,c.default)().siteConfig,t=void 0===e?{}:e;return n.createElement(l.Z,{title:""+t.title,description:"Security focused static analysis tool for Android and Java applications."},n.createElement("header",{className:i("hero hero--primary",u.heroBanner)},n.createElement("div",{className:"container"},n.createElement("img",{className:u.heroLogo,src:"img/MarianaTrench-Icon.png",alt:"Mariana Trench Logo",width:"170"}),n.createElement("h1",{className:"hero__title"},t.title),n.createElement("p",{className:"hero__subtitle"},t.tagline),n.createElement("div",{className:u.buttons},n.createElement(o.default,{className:i("button button--outline button--secondary button--lg",u.buttons),to:(0,s.default)("docs/overview")},"Get Started")))),n.createElement("main",null,n.createElement("div",{className:"container padding-vert--s text--left"},n.createElement("div",{className:"row"},n.createElement("div",{className:"col"},m&&m.length>0&&n.createElement("section",{className:u.features},n.createElement("div",{className:"container"},n.createElement("div",{className:"row"},m.map((function(e){var t=e.title,a=e.imageUrl,r=e.description;return n.createElement(d,{key:t,title:t,imageUrl:a,description:r})}))))))))))}}}]);