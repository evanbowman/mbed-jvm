package java.lang;



public class Runtime {


    private static Runtime runtime = null;


    static {
        new Runtime();
    }


    public static Runtime getRuntime()
    {
        return runtime;
    }


    public native void exit(int code);


    public native void gc();


    public native long totalMemory();


    public native long freeMemory();

}
