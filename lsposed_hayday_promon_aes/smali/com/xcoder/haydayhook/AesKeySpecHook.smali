.class public Lcom/xcoder/haydayhook/AesKeySpecHook;
.super Lde/robv/android/xposed/XC_MethodHook;

.method public constructor <init>()V
    .locals 0
    invoke-direct {p0}, Lde/robv/android/xposed/XC_MethodHook;-><init>()V
    return-void
.end method

.method protected afterHookedMethod(Lde/robv/android/xposed/XC_MethodHook$MethodHookParam;)V
    .locals 5
    :try_start_0
    iget-object v0, p1, Lde/robv/android/xposed/XC_MethodHook$MethodHookParam;->args:[Ljava/lang/Object;
    const/4 v1, 0x0
    aget-object v1, v0, v1
    check-cast v1, [B
    const/4 v2, 0x1
    aget-object v0, v0, v2
    check-cast v0, Ljava/lang/String;
    const-string v2, "AES"
    invoke-virtual {v2, v0}, Ljava/lang/String;->equalsIgnoreCase(Ljava/lang/String;)Z
    move-result v2
    if-eqz v2, :cond_0
    invoke-static {v1}, Lcom/xcoder/haydayhook/LogUtil;->b64([B)Ljava/lang/String;
    move-result-object v2
    new-instance v3, Ljava/lang/StringBuilder;
    invoke-direct {v3}, Ljava/lang/StringBuilder;-><init>()V
    const-string v4, "AES SecretKeySpec alg="
    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v3, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    const-string v0, " len="
    invoke-virtual {v3, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    array-length v0, v1
    invoke-virtual {v3, v0}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;
    const-string v0, " b64="
    invoke-virtual {v3, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v3, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
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
