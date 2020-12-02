// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package android.net;

public class Uri {
  private String scheme;
  private String host;
  private String path;

  public Uri() {}

  public Uri(String scheme, String host, String path) {
    this.scheme = scheme;
    this.host = host;
    this.path = path;
  }

  public static Uri parse(String uri) {
    return new Uri();
  }

  public static String decode(String uri) {
    return "";
  }

  public String getScheme() {
    return this.scheme;
  }

  public String getHost() {
    return this.host;
  }

  public String getQueryParameter(String name) {
    return "";
  }

  public String toString() {
    return this.scheme.concat("://").concat(this.host).concat(this.path);
  }

  public static final class Builder {
    private String scheme;
    private String host;
    private String path;

    public Builder() {}

    public Builder scheme(String scheme) {
      if (scheme != "http" && scheme != "https") {
        throw new RuntimeException("invalid scheme");
      }
      this.scheme = scheme;
      return this;
    }

    public Builder host(String host) {
      this.host = host;
      return this;
    }

    public Builder path(String path) {
      this.path = path;
      return this;
    }

    public Uri build() {
      if (this.scheme == null) {
        throw new RuntimeException("missing scheme");
      }
      if (this.host == null) {
        throw new RuntimeException("missing host");
      }
      if (this.path == null) {
        throw new RuntimeException("missing path");
      }
      return new Uri(this.scheme, this.host, this.path);
    }
  }
}
