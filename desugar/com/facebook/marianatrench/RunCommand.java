/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench;

import com.google.common.io.CharStreams;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

public class RunCommand {
  private static final Logger LOGGER = LogManager.getFormatterLogger();

  public static void run(Process process) {
    try {
      int errorCode = process.waitFor();
      if (errorCode != 0) {
        try (Reader reader = new InputStreamReader(process.getInputStream())) {
          LOGGER.error("Exception when executing a process:\n" + CharStreams.toString(reader));
        } catch (IOException exception) {
          LOGGER.error("Failed to read the process's output!");
        }
      }
    } catch (InterruptedException exception) {
      LOGGER.error("Process execution is interrupted!");
    }
  }
}
