package ourpersist;

class KlassNames {
  public static final int size = 10;

  public String[] names = new String[size];
  public KlassNames next = null;
}

class KlassNameMap {
  private static final int size = 10;
  private static KlassNameMap head = new KlassNameMap();
  private static Object lock = head;

  private String[] names = new String[size];
  private Class<?>[] clazzes = new Class<?>[size];
  private KlassNameMap next = null;

  public static Class<?> search(String target) {
    if (target == null) {
      return null;
    }

    synchronized(lock) {
      KlassNameMap cur = KlassNameMap.head;
      while (cur != null) {
        for (int i = 0; i < KlassNameMap.size; i++) {
          String cur_name = cur.names[i];
          if (cur_name != null && target.equals(cur_name)) {
            return cur.clazzes[i];
          }
        }
        cur = cur.next;
      }
    }

    return null;
  }

  public static void add(Class<?> clazz, boolean allow_dup) {
    String name = clazz.getName();

    synchronized(lock) {
      if (!allow_dup && search(name) != null) {
        return;
      }

      KlassNameMap cur = KlassNameMap.head;
      while (true) {
        for (int i = 0; i < KlassNameMap.size; i++) {
          if (cur.names[i] == null) {
            cur.names[i] = name;
            cur.clazzes[i] = clazz;
            return;
          }
        }

        if (cur.next == null) {
          cur.next = new KlassNameMap();
        }
        cur = cur.next;
      }
    }
  }

}

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

  public static native void init(ClassLoader[] classLoaders);

  // test
  public static int test = 0;
  private static final Recovery theRecovery = new Recovery();
  private native Object test(Object obj, long offset);
  public static Object aaa() {
    System.out.println("aaa");

    String[] names = new String[10];
    int res = getKlassNames(names, names.length / 2);
    System.out.println("getKlassNames " + res);
    for (String name : names) {
      System.out.println(" - " + name);
    }

    System.out.println("getUserClassLoaders " + getUserClassLoaders(new ClassLoader[10]));
    return theRecovery.test(null, 0);
  }
  public static native Object test2(long offset);
  public static native void getLoader(ClassLoader[] obj);
  public static native void createDramCopyTest(Object[] root_head);

  // native
  private static native int getKlassNames(String[] names, int start_pos);
  private static native int getUserClassLoaders(ClassLoader[] loaders);

  // java
  private static KlassNames getKlassNames() {
    KlassNames names = new KlassNames();

    int klass_names_pos = 0;
    KlassNames cname = names;
    while (true) {
      int len = getKlassNames(cname.names, klass_names_pos);
      klass_names_pos += len;

      System.out.println("[Recovery (Java)] len " + len);

      if (len < KlassNames.size) {
        break;
      }

      KlassNames next = new KlassNames();
      cname.next = next;
      cname = next;
    }

    return names;
  }

  private static ClassLoader[] getUserClassLoaders() {
    final int init_size  = 10;
    final int block_size = 10;

    ClassLoader[] user_class_loader;
    int total_size = init_size;

    while (true) {
      user_class_loader = new ClassLoader[total_size];

      try {
        int size = getUserClassLoaders(user_class_loader);
        if (size <= total_size) break;

        // retry
        total_size = (size / block_size) * block_size + 1;
      } catch (Exception e) {
        System.err.println(e);
      }
    }

    return user_class_loader;
  }

  private static boolean loadClass(ClassLoader[] user_class_loaders, String name) {
    if (name == null) return false;

    // DEBUG:
    System.out.println("[Recovery (Java)] load class: " + name);

    Class<?> clazz = null;
    try {
      clazz = Class.forName(name);
    } catch (Exception e) {
      System.err.println(e);
    }
    if (clazz != null) return true;

    for (ClassLoader cld : user_class_loaders) {
      if (cld == null) continue;

      try {
        clazz = cld.loadClass(name);
      } catch (Exception e) {
        System.err.println(e);
        continue;
      }

      break;
    }

    return clazz != null;
  }

  private static void loadClasses() throws Exception {
    KlassNames names = getKlassNames();
    ClassLoader[] user_class_loaders = getUserClassLoaders();

    while (names != null) {
      for (int i = 0; i < KlassNames.size; i++) {
        String klass_name = names.names[i];
        if (klass_name == null) continue;

        boolean is_load = loadClass(user_class_loaders, klass_name);
        if (!is_load) throw new RecoveryNotFindClassException("");
      }
      names = names.next;
    }
  }

  public static void recovery() throws Exception {
    loadClasses();
  }
}
