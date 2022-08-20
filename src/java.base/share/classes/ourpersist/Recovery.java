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

  public static void init(String nvmFilePath) {
    if (nvmFilePath == null) {
      throw new NullPointerException("nvmFilePath is null");
    }
    if (!exists(nvmFilePath)) {
      createNvmFile(nvmFilePath);
    }

    initInternal(nvmFilePath);
  }

  public static void recovery(ClassLoader[] classLoaders, String nvmFilePath) {
    String[] classNames = nvmCopyClassNames(nvmFilePath);

    Class<?>[] classes = new Class<?>[classNames.length];
    for (int i = 0; i < classNames.length; i++) {
      String className = classNames[i];
      for (ClassLoader cld : classLoaders) {
        try {
          classes[i] = cld.loadClass(className);
          break;
        } catch (ClassNotFoundException e) {}
      }
    }

    Object[] dramCopyList = new Object[128];
    createDramCopy(dramCopyList, classes, nvmFilePath);

    recoveryDramCopy(dramCopyList, classes, nvmFilePath);
  }
}
