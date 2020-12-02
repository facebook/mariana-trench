// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package java.lang;

public class Thread implements Runnable {

  private Runnable target;

  public Thread(Runnable runnable) {
    this.target = runnable;
  }

  @Override
  public void run() {
    if (target != null) {
      target.run();
    }
  }

  public void start() {}
}
