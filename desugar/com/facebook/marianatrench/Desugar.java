// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Comparator;
import java.util.Enumeration;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.stream.Stream;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;

public class Desugar {
  private static final Logger LOGGER = LogManager.getFormatterLogger();

  public static void main(final String[] arguments) throws IOException, FileNotFoundException {
    LOGGER.info("Desugaring file %s into file %s", arguments[0], arguments[1]);

    Path temporaryDirectory = Files.createTempDirectory("");

    try (JarFile inputJar = new JarFile(arguments[0])) {
      Enumeration<JarEntry> entries = inputJar.entries();
      while (entries.hasMoreElements()) {
        JarEntry entry = entries.nextElement();
        File file = new File(temporaryDirectory + File.separator + entry.getName());
        if (entry.isDirectory()) {
          file.mkdir();
          continue;
        }
        try (InputStream inputStream = inputJar.getInputStream(entry)) {
          while (inputStream.available() > 0 && file.getName().endsWith(".class")) {
            try (FileOutputStream outputStream = new FileOutputStream(file)) {
              ClassReader reader = new ClassReader(inputStream);
              // Must not set COMPUTE_FRAMES, because some classes are loaded from classloaders that
              // are not the bootstrap classloader and this would cause ASM to be unable to find
              // those classes.
              ClassWriter writer = new ClassWriter(/* no flags */ 0);
              NestVisitor nestVisitor = new NestVisitor(writer);
              MethodHandleVisitor methodHandleVisitor = new MethodHandleVisitor(nestVisitor);
              reader.accept(methodHandleVisitor, /* no options */ 0);

              outputStream.write(writer.toByteArray());
            }
          }
        }
      }
    }

    String[] command = {"jar", "cf", arguments[1], "."};
    RunCommand.run(
        Runtime.getRuntime()
            .exec(command, null, new File(temporaryDirectory.toAbsolutePath().toString())));

    try (Stream<Path> walk = Files.walk(temporaryDirectory)) {
      walk.sorted(Comparator.reverseOrder()).map(Path::toFile).forEach(File::delete);
    }
  }
}
