/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  static class Event {
    String name;
    String annotation;
  }

  interface Logger {
    Event mark(String name, String annotation);
  }

  // Declares `mark` again without implementing it.
  interface ExtendedLogger extends Logger {
    @Override
    Event mark(String name, String annotation);
  }

  static class ExtendedLoggerImpl implements ExtendedLogger {
    @Override
    public Event mark(String name, String annotation) {
      Event event = new Event();
      event.name = name;
      event.annotation = annotation;
      return event;
    }
  }

  interface Reporter {
    Event report(String name, String annotation);
  }

  // Redex's `RemoveUnreachablePass` leaves this shape when a class is never
  // instantiated: its method becomes abstract.
  abstract static class AbstractReporter implements Reporter {
    @Override
    public abstract Event report(String name, String annotation);
  }

  static class ReporterImpl extends AbstractReporter {
    @Override
    public Event report(String name, String annotation) {
      Event event = new Event();
      event.name = name;
      event.annotation = annotation;
      return event;
    }
  }

  public static void noIssueThroughLogger(Logger logger) {
    Event event = logger.mark("name", (String) Origin.source());
    Origin.sink(event.name);
  }

  public static void issueThroughLogger(Logger logger) {
    Event event = logger.mark("name", (String) Origin.source());
    Origin.sink(event.annotation);
  }

  public static void noIssueThroughReporter(Reporter reporter) {
    Event event = reporter.report("name", (String) Origin.source());
    Origin.sink(event.name);
  }

  public static void issueThroughReporter(Reporter reporter) {
    Event event = reporter.report("name", (String) Origin.source());
    Origin.sink(event.annotation);
  }
}
