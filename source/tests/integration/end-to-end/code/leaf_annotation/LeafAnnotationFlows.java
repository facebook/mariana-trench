// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

import com.facebook.graphql.calls.LocationUpdateData;

public class LeafAnnotationFlows {
  public static native String nativeGetLocationName();

  private String storedLocation;

  public String test() {
    String s = nativeGetLocationName();
    intermediateFunction1(s);
    this.storedLocation = s;
    return s;
  }

  public void callback(String locationName) {
    intermediateFunction1(locationName);
  }

  public void intermediateFunction1(String s) {
    intermediateFunction2(s);
  }

  public void intermediateFunction2(String s) {
    sendToGraphQL(s);
  }

  public void sendToGraphQL(String s) {
    LocationUpdateData input = new LocationUpdateData();
    input.setPlaceId(s);
  }
}
