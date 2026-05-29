package com.facebook.marianatrench.integrationtests;

class SensitiveApi {
  void execute() {}
}

class CallChainService {
  void request(String method) {
    if (method.equals("POST")) {
      new SensitiveApi().execute();
    }
  }
}

class Exported {
  void entry() {
    new CallChainService().request("GET");
  }
}

public class CallChainGuarded {
  static void callChainIssue() {
    new Exported().entry();
  }
}
