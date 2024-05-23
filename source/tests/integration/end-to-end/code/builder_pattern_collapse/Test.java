/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Test {
  // 22 fields, Heuristics::kGenerationMaxOutputPathLeaves = 20
  public Test a;
  public Test b;
  public Test c;
  public Test d;
  public Test e;
  public Test f;
  public Test g;
  public Test h;
  public Test i;
  public Test j;
  public Test k;
  public Test l;
  public Test m;
  public Test n;
  public Test o;
  public Test p;
  public Test q;
  public Test r;
  public Test s;
  public Test t;
  public Test u;
  public Test v;

  public Test() {}

  public Test(
      Test a,
      Test b,
      Test c,
      Test d,
      Test e,
      Test f,
      Test g,
      Test h,
      Test i,
      Test j,
      Test k,
      Test l,
      Test m,
      Test n,
      Test o,
      Test p,
      Test q,
      Test r,
      Test s,
      Test t,
      Test u,
      Test v) {
    this.a = a;
    this.b = b;
    this.c = c;
    this.d = d;
    this.e = e;
    this.f = f;
    this.g = g;
    this.h = h;
    this.i = i;
    this.j = j;
    this.k = k;
    this.l = l;
    this.m = m;
    this.n = n;
    this.o = o;
    this.p = p;
    this.q = q;
    this.r = r;
    this.s = s;
    this.t = t;
    this.u = u;
    this.v = v;
  }

  private static Object differentSource() {
    return new Object();
  }

  private static void differentSink(Object obj) {}

  public static final class Builder {
    public Test a;
    public Test b;
    public Test c;
    public Test d;
    public Test e;
    public Test f;
    public Test g;
    public Test h;
    public Test i;
    public Test j;
    public Test k;
    public Test l;
    public Test m;
    public Test n;
    public Test o;
    public Test p;
    public Test q;
    public Test r;
    public Test s;
    public Test t;
    public Test u;
    public Test v;

    public Builder setA(Test a) {
      this.a = a;
      return this;
    }

    public Builder setB(Test b) {
      this.b = b;
      return this;
    }

    public Builder setC(Test c) {
      this.c = c;
      return this;
    }

    public Builder setD(Test d) {
      this.d = d;
      return this;
    }

    public Builder setE(Test e) {
      this.e = e;
      return this;
    }

    public Builder setF(Test f) {
      this.f = f;
      return this;
    }

    public Builder setG(Test g) {
      this.g = g;
      return this;
    }

    public Builder setH(Test h) {
      this.h = h;
      return this;
    }

    public Builder setI(Test i) {
      this.i = i;
      return this;
    }

    public Builder setJ(Test j) {
      this.j = j;
      return this;
    }

    public Builder setK(Test k) {
      this.k = k;
      return this;
    }

    public Builder setL(Test l) {
      this.l = l;
      return this;
    }

    public Builder setM(Test m) {
      this.m = m;
      return this;
    }

    public Builder setN(Test n) {
      this.n = n;
      return this;
    }

    public Builder setO(Test o) {
      this.o = o;
      return this;
    }

    public Builder setP(Test p) {
      this.p = p;
      return this;
    }

    public Builder setQ(Test q) {
      this.q = q;
      return this;
    }

    public Builder setR(Test r) {
      this.r = r;
      return this;
    }

    public Builder setS(Test s) {
      this.s = s;
      return this;
    }

    public Builder setT(Test t) {
      this.t = t;
      return this;
    }

    public Builder setU(Test u) {
      this.u = u;
      return this;
    }

    public Builder setV(Test v) {
      this.v = v;
      return this;
    }

    public Test build() {
      return new Test(
          this.a, this.b, this.c, this.d, this.e, this.f, this.g, this.h, this.i, this.j, this.k,
          this.l, this.m, this.n, this.o, this.p, this.q, this.r, this.s, this.t, this.u, this.v);
    }
  }

  public static Test collapseOnBuilder() {
    // Resulting taint tree will have caller port Return
    return (new Builder())
        .setA((Test) Origin.source())
        .setB((Test) Test.differentSource())
        .setC((Test) Origin.source())
        .setD((Test) Origin.source())
        .setE((Test) Origin.source())
        .setF((Test) Origin.source())
        .setG((Test) Origin.source())
        .setH((Test) Origin.source())
        .setI((Test) Origin.source())
        .setJ((Test) Origin.source())
        .setK((Test) Origin.source())
        .setL((Test) Origin.source())
        .setM((Test) Origin.source())
        .setN((Test) Origin.source())
        .setO((Test) Origin.source())
        .setP((Test) Origin.source())
        .setQ((Test) Origin.source())
        .setR((Test) Origin.source())
        .setS((Test) Origin.source())
        .setT((Test) Origin.source())
        .setU((Test) Origin.source())
        .build(); // 21 fields Heuristics::kGenerationMaxOutputPathLeaves = 20
  }

  public static Test noCollapseOnBuilder() {
    // Resulting taint tree will have caller port Return.a and Return.b
    return (new Builder()).setA((Test) Origin.source()).setB((Test) Test.differentSource()).build();
  }

  public static void testCollapseOnBuilder() {
    Test output = collapseOnBuilder();

    // issue: a has "Source" and b has "DifferentSource"
    Origin.sink(output.a);
    Test.differentSink(output.b);

    // Also issue: false positive due to collapsing
    Test.differentSink(output.a);
    Origin.sink(output.b);
    Origin.sink(output.v);
    Test.differentSink(output.v);
  }

  public static void testNoCollapseOnBuilder() {
    Test output = noCollapseOnBuilder();

    // issue: a has "Source" and b has "DifferentSource"
    Origin.sink(output.a);
    Test.differentSink(output.b);

    // no issue, since no collapsing occurs
    Test.differentSink(output.a);
    Origin.sink(output.b);
    Origin.sink(output.v);
    Test.differentSink(output.v);
  }
}
