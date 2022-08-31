package ourpersist;

class RecoveryException extends Exception {
	private static final long serialVersionUID = 1L;
	RecoveryException(String msg) {
		super(msg);
	}
}

class RecoveryNotFindClassException extends RecoveryException {
	private static final long serialVersionUID = 1L;
	RecoveryNotFindClassException(String msg) {
		super(msg);
	}
}

class RecoveryRootFieldNotNullException extends RecoveryException {
	private static final long serialVersionUID = 1L;
	RecoveryRootFieldNotNullException(String msg) {
		super(msg);
	}
}

public class Recovery {
  private static native void registerNatives();
  static {
    registerNatives();
  }
  Recovery() {
    super();
  }

  public static native void initNvmFile(String nvmFilePath);
  public static native boolean hasEnableNvmData(String nvmFilePath);
  private static native void disableNvmData(String nvmFilePath);

  // public static void init(String nvmFilePath);
  private static native void initInternal(String nvmFilePath);

  // public static void recovery(ClassLoader[] classLoaders, String nvmFilePath);
  private static native String[] nvmCopyClassNames(String nvmFilePath);
  private static native void createDramCopy(Object[] dramCopyList, Class<?>[] classes, String nvmFilePath);
  private static native void recoveryDramCopy(Object[] dramCopyList, Class<?>[] classes, String nvmFilePath);

  public static native void killMe();
  public static native String mode();

  public static void init(String nvmFilePath) {
    if (nvmFilePath == null) {
      throw new NullPointerException("nvmFilePath is null");
    }
    initInternal(nvmFilePath);
  }

  private static Class<?> loadClass(ClassLoader[] classLoaders, String className) {
    for (ClassLoader classLoader : classLoaders) {
      try {
        return Class.forName(className, true, classLoader);
      } catch (ClassNotFoundException e) {}
    }
    return null;
  }

  public static void recovery(ClassLoader[] classLoaders, String nvmFilePath) throws Exception {
    System.out.println("[Recovery] start");
    if (!hasEnableNvmData(nvmFilePath)) {
      throw new RecoveryException("nvm data is not enabled");
    }

    disableNvmData(nvmFilePath);

    System.out.println("[Recovery] get class names");
    String[] classNames = nvmCopyClassNames(nvmFilePath);

    System.out.println("[Recovery] load classes");
    Class<?>[] classes = new Class<?>[classNames.length];
    for (int i = 0; i < classNames.length; i++) {
      String className = classNames[i].replaceAll("/", ".");

      classes[i] = loadClass(classLoaders, className);
      if (classes[i] == null) {
        throw new RecoveryNotFindClassException("class not found: " + className);
      }
    }

    // WARNING: This is a hack to avoid garbage collection of the objects.
    //          These objects are incomplete and should not be used in java code.
    System.out.println("[Recovery] create dram copy");
    Object[] dramCopyList = new Object[16]; // DEBUG:
    long createDramCopyTime = System.currentTimeMillis();
    createDramCopy(dramCopyList, classes, nvmFilePath);
    System.out.println("[Recovery] create dram copy time: " + (System.currentTimeMillis() - createDramCopyTime));
    // DEBUG:
    {
      int count_true = 0;
      int count_false = 0;
      Object[] cur = dramCopyList;
      while (cur != null) {
        for (int i = 0; i < dramCopyList.length - 1; i++) {
          if (cur[i] != null) {
            count_true++;
          } else {
            count_false++;
          }
        }
        cur = (Object[])cur[dramCopyList.length - 1];
      }
      System.out.println("[Recovery] dram copy count: " + count_true + " " + count_false);
    }

    System.out.println("[Recovery] recovery dram copy");
    long recoveryDramCopyTime = System.currentTimeMillis();
    recoveryDramCopy(dramCopyList, classes, nvmFilePath);
    System.out.println("[Recovery] recovery dram copy time: " + (System.currentTimeMillis() - recoveryDramCopyTime));
    // DEBUG:
    {
      Object[] cur = dramCopyList;
      while (cur != null) {
        for (int i = 0; i < dramCopyList.length - 1; i++) {
          //System.out.println(cur[i]);
        }
        cur = (Object[])cur[dramCopyList.length - 1];
      }
    }

    System.out.println("[Recovery] finish");
  }
}
