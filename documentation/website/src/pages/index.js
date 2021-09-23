/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @format
 */

import React from 'react';
import clsx from 'clsx';
import Layout from '@theme/Layout';
import Link from '@docusaurus/Link';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import useBaseUrl from '@docusaurus/useBaseUrl';
import styles from './styles.module.css';

const features = [
  {
    title: 'Android and Java',
    description: (
      <>
        Find security vulnerabilities in Android and Java applications. Mariana
        Trench analyzes Dalvik bytecode.
      </>
    ),
  },
  {
    title: 'Fast',
    description: (
      <>
        Mariana Trench is built to run fast on large codebases (10s of millions
        of lines of code). It can find vulnerabilities as code changes, before
        it ever lands in your repository.
      </>
    ),
  },
  {
    title: 'Customizable',
    description: (
      <>
        Adapt Mariana Trench to your needs. Find the vulnerabilities that you
        care about by teaching Mariana Trench where data comes from and where
        you do not want it to go.
      </>
    ),
  },
];

function Feature({imageUrl, title, description}) {
  const imgUrl = useBaseUrl(imageUrl);
  return (
    <div className={clsx('col col--4', styles.feature)}>
      {imgUrl && (
        <div className="text--center">
          <img className={styles.featureImage} src={imgUrl} alt={title} />
        </div>
      )}
      <h3>{title}</h3>
      <p>{description}</p>
    </div>
  );
}

function Home() {
  const context = useDocusaurusContext();
  const {siteConfig = {}} = context;
  return (
    <Layout
      title={`${siteConfig.title}`}
      description="Security focused static analysis tool for Android and Java applications.">
      <header className={clsx('hero hero--primary', styles.heroBanner)}>
        <div className="container">
          <img
            className={styles.heroLogo}
            src="img/MarianaTrench-Icon.png"
            alt="Mariana Trench Logo"
            width="170"
          />
          <h1 className="hero__title">{siteConfig.title}</h1>
          <p className="hero__subtitle">{siteConfig.tagline}</p>
          <div className={styles.buttons}>
            <Link
              className={clsx(
                'button button--outline button--secondary button--lg',
                styles.buttons,
              )}
              to={useBaseUrl('docs/overview')}>
              Get Started
            </Link>
          </div>
        </div>
      </header>
      <main>
        <div className="container padding-vert--s text--left">
          <div className="row">
            <div className="col">
              {features && features.length > 0 && (
                <section className={styles.features}>
                  <div className="container">
                    <div className="row">
                      {features.map(({title, imageUrl, description}) => (
                        <Feature
                          key={title}
                          title={title}
                          imageUrl={imageUrl}
                          description={description}
                        />
                      ))}
                    </div>
                  </div>
                </section>
              )}
            </div>
          </div>
        </div>
      </main>
    </Layout>
  );
}

export default Home;
