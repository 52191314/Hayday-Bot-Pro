.class public Lcom/xcoder/haydayhook/CipherGetInstanceHook;
.super Lde/robv/android/xposed/XC_MethodHook;

.method public constructor <init>()V
    .locals 0
    invoke-direct {p0}, Lde/robv/android/xposed/XC_MethodHook;-><init>()V
    return-void
.end method

.method protected afterHookedMethod(Lde/robv/android/xposed/XC_MethodHook$MethodHookParam;)V
    .locals 4
    :try_start_0
    iget-object v0, p1, Lde/robv/android/xposed/XC_MethodHook$MethodHookParam;->args:[Ljava/lang/Object;
    const/4 v1, 0x0
    aget-object v0, v0, v1
    check-cast v0, Ljava/lang/String;
    const-string v1, "AES"
    invoke-virtual {v0, v1}, Ljava/lang/String;->contains(Ljava/lang/CharSequence;)Z
    move-result v1
    if-eqz v1, :cond_0
    new-instance v2, Ljava/lang/StringBuilder;
    invoke-direct {v2}, Ljava/lang/StringBuilder;-><init>()V
    const-string v3, "Cipher.getInstance "
    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v2, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v2}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
    move-result-object v0
    invoke-static {v0}, Lcom/xcoder/haydayhook/LogUtil;->log(Ljava/lang/String;)V
    :cond_0
    :try_end_0
    .catch Ljava/lang/Throwable; {:try_start_0 .. :try_end_0} :catch_0
    return-void
    :catch_0
    move-exception v0
    invoke-static {v0}, Lde/robv/android/xposed/XposedBridge;->log(Ljava/lang/Throwable;)V
    return-void
.end method
