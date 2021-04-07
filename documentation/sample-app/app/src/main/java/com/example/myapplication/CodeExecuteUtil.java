/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.example.myapplication;

import android.util.Log;
import java.io.IOException;

public class CodeExecuteUtil {
  public static void execute(String str) {
    try {
      new ProcessBuilder(str).start();
    } catch (IOException e) {
      Log.e("CodeExecuteUtil", "Failed to execute" + str);
    }
  }
}
