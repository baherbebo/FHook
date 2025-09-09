package top.feadre.fhook;

/**
 * 测试对象 属性提取
 */
public class TObject {
    private String name;
    private int age;

    // 一些静态字段，便于测试静态读写
    private static int count;
    private static String staticName;
    private static int staticAge;
    private static int staticCount;

    public TObject() { this("noname", 0); }
    public TObject(String name, int age) { this.name = name; this.age = age; count++; }

    // getter/setter
    public String getName() { return name; }
    public TObject setName(String name) { this.name = name; return this; }
    public int getAge() { return age; }
    public TObject setAge(int age) { this.age = age; return this; }

    // 静态字段操作（便于 hook 静态场景）
    public static int getCount() { return count; }
    public static void setStaticName(String v) { staticName = v; }
    public static String getStaticName() { return staticName; }
    public static void setStaticAge(int v) { staticAge = v; }
    public static int getStaticAge() { return staticAge; }
    public static void incStaticCount() { staticCount++; }
    public static int getStaticCount() { return staticCount; }

    @Override public String toString() { return "TObject{name='" + name + "', age=" + age + "}"; }
}
