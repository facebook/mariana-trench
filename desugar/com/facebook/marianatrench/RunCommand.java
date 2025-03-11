/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench;

import com.facebook.infer.annotation.Nullsafe;
import com.google.common.io.CharStreams;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;

@Nullsafe(Nullsafe.Mode.LOCAL)
public class RunCommand {
  public static void run(Process process) {
    try {
      int errorCode = process.waitFor();
      if (errorCode != 0) {
        try (Reader reader = new InputStreamReader(process.getInputStream())) {
          System.out.printf("Exception when executing a process:%n" + CharStreams.toString(reader));
        } catch (IOException exception) {
          System.out.println("Failed to read the process's output!");
        }
      }
    } catch (InterruptedException exception) {
      System.out.println("Process execution is interrupted!");
    }
  }
}
