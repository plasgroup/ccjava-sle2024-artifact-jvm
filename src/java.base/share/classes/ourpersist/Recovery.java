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

  public static native boolean exists(String nvmFilePath);

  // public static void init(String nvmFilePath);
  public static native void initInternal(String nvmFilePath);
  public static native void createNvmFile(String nvmFilePath);

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
    if (!exists(nvmFilePath)) {
      createNvmFile(nvmFilePath);
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
    String[] classNames = nvmCopyClassNames(nvmFilePath);

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
    Object[] dramCopyList = new Object[8];
    createDramCopy(dramCopyList, classes, nvmFilePath);
    for (Object dramCopy : dramCopyList) {
      System.out.println(dramCopy != null);
    }

    recoveryDramCopy(dramCopyList, classes, nvmFilePath);
    for (Object dramCopy : dramCopyList) {
      System.out.println(dramCopy);
    }
  }
}
