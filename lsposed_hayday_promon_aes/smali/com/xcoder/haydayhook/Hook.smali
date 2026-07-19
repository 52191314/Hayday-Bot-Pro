.class public Lcom/xcoder/haydayhook/Hook;
.super Ljava/lang/Object;
.implements Lde/robv/android/xposed/IXposedHookLoadPackage;

.method public constructor <init>()V
    .locals 0
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method private static hookCtor(Ljava/lang/String;Ljava/lang/ClassLoader;[Ljava/lang/Object;Ljava/lang/String;)V
    .locals 3
    :try_start_0
    invoke-static {p0, p1, p2}, Lde/robv/android/xposed/XposedHelpers;->findAndHookConstructor(Ljava/lang/String;Ljava/lang/ClassLoader;[Ljava/lang/Object;)Lde/robv/android/xposed/XC_MethodHook$Unhook;
    new-instance v0, Ljava/lang/StringBuilder;
    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V
    const-string v1, "HayDayHook ctor hooked "
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0, p3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
    move-result-object v0
    invoke-static {v0}, Lcom/xcoder/haydayhook/LogUtil;->log(Ljava/lang/String;)V
    :try_end_0
    .catch Ljava/lang/Throwable; {:try_start_0 .. :try_end_0} :catch_0
    return-void
    :catch_0
    move-exception v0
    new-instance v1, Ljava/lang/StringBuilder;
    invoke-direct {v1}, Ljava/lang/StringBuilder;-><init>()V
    const-string v2, "HayDayHook ctor hook failed "
    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v1, p3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    const-string v2, ": "
    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v1, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/Object;)Ljava/lang/StringBuilder;
    invoke-virtual {v1}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
    move-result-object v1
    invoke-static {v1}, Lcom/xcoder/haydayhook/LogUtil;->log(Ljava/lang/String;)V
    invoke-static {v0}, Lde/robv/android/xposed/XposedBridge;->log(Ljava/lang/Throwable;)V
    return-void
.end method

.method private static hookMethod(Ljava/lang/String;Ljava/lang/ClassLoader;Ljava/lang/String;[Ljava/lang/Object;Ljava/lang/String;)V
    .locals 3
    :try_start_0
    invoke-static {p0, p1, p2, p3}, Lde/robv/android/xposed/XposedHelpers;->findAndHookMethod(Ljava/lang/String;Ljava/lang/ClassLoader;Ljava/lang/String;[Ljava/lang/Object;)Lde/robv/android/xposed/XC_MethodHook$Unhook;
    new-instance v0, Ljava/lang/StringBuilder;
    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V
    const-string v1, "HayDayHook method hooked "
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0, p4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
    move-result-object v0
    invoke-static {v0}, Lcom/xcoder/haydayhook/LogUtil;->log(Ljava/lang/String;)V
    :try_end_0
    .catch Ljava/lang/Throwable; {:try_start_0 .. :try_end_0} :catch_0
    return-void
    :catch_0
    move-exception v0
    new-instance v1, Ljava/lang/StringBuilder;
    invoke-direct {v1}, Ljava/lang/StringBuilder;-><init>()V
    const-string v2, "HayDayHook method hook failed "
    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v1, p4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    const-string v2, ": "
    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v1, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/Object;)Ljava/lang/StringBuilder;
    invoke-virtual {v1}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
    move-result-object v1
    invoke-static {v1}, Lcom/xcoder/haydayhook/LogUtil;->log(Ljava/lang/String;)V
    invoke-static {v0}, Lde/robv/android/xposed/XposedBridge;->log(Ljava/lang/Throwable;)V
    return-void
.end method

.method public handleLoadPackage(Lde/robv/android/xposed/callbacks/XC_LoadPackage$LoadPackageParam;)V
    .locals 8
    iget-object v0, p1, Lde/robv/android/xposed/callbacks/XC_LoadPackage$LoadPackageParam;->packageName:Ljava/lang/String;
    const-string v1, "com.supercell.hayday"
    invoke-virtual {v1, v0}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v0
    if-nez v0, :cond_target
    return-void

    :cond_target
    const-string v0, "HayDayHook active in com.supercell.hayday"
    invoke-static {v0}, Lcom/xcoder/haydayhook/LogUtil;->log(Ljava/lang/String;)V
    iget-object v1, p1, Lde/robv/android/xposed/callbacks/XC_LoadPackage$LoadPackageParam;->classLoader:Ljava/lang/ClassLoader;

    const/4 v2, 0x2
    new-array v3, v2, [Ljava/lang/Object;
    const/4 v4, 0x0
    const-class v5, Landroid/content/Context;
    aput-object v5, v3, v4
    const/4 v5, 0x1
    new-instance v6, Lcom/xcoder/haydayhook/ReturnVoidHook;
    invoke-direct {v6}, Lcom/xcoder/haydayhook/ReturnVoidHook;-><init>()V
    aput-object v6, v3, v5
    const-string v6, "com.supercell.titan.PromonTitan"
    const-string v7, "addObserver"
    const-string v0, "PromonTitan.addObserver(Context)"
    invoke-static {v6, v1, v7, v3, v0}, Lcom/xcoder/haydayhook/Hook;->hookMethod(Ljava/lang/String;Ljava/lang/ClassLoader;Ljava/lang/String;[Ljava/lang/Object;Ljava/lang/String;)V

    new-array v3, v2, [Ljava/lang/Object;
    const-class v6, Ljava/lang/String;
    aput-object v6, v3, v4
    new-instance v6, Lcom/xcoder/haydayhook/ReturnVoidHook;
    invoke-direct {v6}, Lcom/xcoder/haydayhook/ReturnVoidHook;-><init>()V
    aput-object v6, v3, v5
    const-string v6, "bjnr.ae"
    const-string v7, "a"
    const-string v0, "bjnr.ae.a(String) fatal reporter"
    invoke-static {v6, v1, v7, v3, v0}, Lcom/xcoder/haydayhook/Hook;->hookMethod(Ljava/lang/String;Ljava/lang/ClassLoader;Ljava/lang/String;[Ljava/lang/Object;Ljava/lang/String;)V

    const/4 v0, 0x1
    new-array v3, v0, [Ljava/lang/Object;
    new-instance v6, Lcom/xcoder/haydayhook/ReturnVoidHook;
    invoke-direct {v6}, Lcom/xcoder/haydayhook/ReturnVoidHook;-><init>()V
    aput-object v6, v3, v4
    const-string v6, "bjnr.ae"
    const-string v7, "d"
    const-string v2, "bjnr.ae.d() report thread"
    invoke-static {v6, v1, v7, v3, v2}, Lcom/xcoder/haydayhook/Hook;->hookMethod(Ljava/lang/String;Ljava/lang/ClassLoader;Ljava/lang/String;[Ljava/lang/Object;Ljava/lang/String;)V

    new-array v3, v0, [Ljava/lang/Object;
    new-instance v6, Lcom/xcoder/haydayhook/ReturnFalseHook;
    invoke-direct {v6}, Lcom/xcoder/haydayhook/ReturnFalseHook;-><init>()V
    aput-object v6, v3, v4
    const-string v6, "bjnr.C"
    const-string v7, "b"
    const-string v2, "bjnr.C.b() boolean check"
    invoke-static {v6, v1, v7, v3, v2}, Lcom/xcoder/haydayhook/Hook;->hookMethod(Ljava/lang/String;Ljava/lang/ClassLoader;Ljava/lang/String;[Ljava/lang/Object;Ljava/lang/String;)V

    const/4 v1, 0x0
    const/4 v2, 0x3
    new-array v3, v2, [Ljava/lang/Object;
    const-class v6, [B
    aput-object v6, v3, v4
    const-class v6, Ljava/lang/String;
    aput-object v6, v3, v5
    new-instance v6, Lcom/xcoder/haydayhook/AesKeySpecHook;
    invoke-direct {v6}, Lcom/xcoder/haydayhook/AesKeySpecHook;-><init>()V
    const/4 v7, 0x2
    aput-object v6, v3, v7
    const-string v6, "javax.crypto.spec.SecretKeySpec"
    const-string v2, "SecretKeySpec(byte[],String)"
    invoke-static {v6, v1, v3, v2}, Lcom/xcoder/haydayhook/Hook;->hookCtor(Ljava/lang/String;Ljava/lang/ClassLoader;[Ljava/lang/Object;Ljava/lang/String;)V

    const/4 v2, 0x2
    new-array v3, v2, [Ljava/lang/Object;
    const-class v6, Ljava/lang/String;
    aput-object v6, v3, v4
    new-instance v6, Lcom/xcoder/haydayhook/CipherGetInstanceHook;
    invoke-direct {v6}, Lcom/xcoder/haydayhook/CipherGetInstanceHook;-><init>()V
    aput-object v6, v3, v5
    const-string v6, "javax.crypto.Cipher"
    const-string v7, "getInstance"
    const-string v2, "Cipher.getInstance(String)"
    invoke-static {v6, v1, v7, v3, v2}, Lcom/xcoder/haydayhook/Hook;->hookMethod(Ljava/lang/String;Ljava/lang/ClassLoader;Ljava/lang/String;[Ljava/lang/Object;Ljava/lang/String;)V

    const/4 v2, 0x3
    new-array v3, v2, [Ljava/lang/Object;
    sget-object v6, Ljava/lang/Integer;->TYPE:Ljava/lang/Class;
    aput-object v6, v3, v4
    const-class v6, Ljava/security/Key;
    aput-object v6, v3, v5
    new-instance v6, Lcom/xcoder/haydayhook/CipherInitHook;
    invoke-direct {v6}, Lcom/xcoder/haydayhook/CipherInitHook;-><init>()V
    aput-object v6, v3, v7
    const-string v6, "javax.crypto.Cipher"
    const-string v2, "init"
    const-string v0, "Cipher.init(int,Key)"
    invoke-static {v6, v1, v2, v3, v0}, Lcom/xcoder/haydayhook/Hook;->hookMethod(Ljava/lang/String;Ljava/lang/ClassLoader;Ljava/lang/String;[Ljava/lang/Object;Ljava/lang/String;)V

    const/4 v0, 0x2
    new-array v3, v0, [Ljava/lang/Object;
    const-class v6, [B
    aput-object v6, v3, v4
    new-instance v6, Lcom/xcoder/haydayhook/CipherDoFinalHook;
    invoke-direct {v6}, Lcom/xcoder/haydayhook/CipherDoFinalHook;-><init>()V
    aput-object v6, v3, v5
    const-string v6, "javax.crypto.Cipher"
    const-string v2, "doFinal"
    const-string v0, "Cipher.doFinal(byte[])"
    invoke-static {v6, v1, v2, v3, v0}, Lcom/xcoder/haydayhook/Hook;->hookMethod(Ljava/lang/String;Ljava/lang/ClassLoader;Ljava/lang/String;[Ljava/lang/Object;Ljava/lang/String;)V

    return-void
.end method
