// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package java.lang;

public class String implements CharSequence {

  public String valueOf(Object object) {
    return "";
  }

  public String substring(int start) {
    return "";
  }

  public String substring(int start, int end) {
    return "";
  }

  public int indexOf(String other) {
    return 0;
  }

  public boolean startsWith(String other) {
    return true;
  }

  public int length() {
    return 0;
  }

  public boolean contains(String other) {
    return true;
  }

  public boolean isEmpty() {
    return true;
  }

  public char charAt(int index) {
    return 'a';
  }

  public CharSequence subSequence(int start, int end) {
    return null;
  }

  public String concat(String other) {
    if (isEmpty()) {
      return this;
    } else {
      return other;
    }
  }
}
