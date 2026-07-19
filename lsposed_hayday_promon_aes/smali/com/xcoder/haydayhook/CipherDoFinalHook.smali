.class public Lcom/xcoder/haydayhook/CipherDoFinalHook;
.super Lde/robv/android/xposed/XC_MethodHook;

.method public constructor <init>()V
    .locals 0
    invoke-direct {p0}, Lde/robv/android/xposed/XC_MethodHook;-><init>()V
    return-void
.end method

.method protected afterHookedMethod(Lde/robv/android/xposed/XC_MethodHook$MethodHookParam;)V
    .locals 8
    :try_start_0
    iget-object v0, p1, Lde/robv/android/xposed/XC_MethodHook$MethodHookParam;->thisObject:Ljava/lang/Object;
    check-cast v0, Ljavax/crypto/Cipher;
    invoke-virtual {v0}, Ljavax/crypto/Cipher;->getAlgorithm()Ljava/lang/String;
    move-result-object v1
    const-string v2, "AES"
    invoke-virtual {v1, v2}, Ljava/lang/String;->contains(Ljava/lang/CharSequence;)Z
    move-result v2
    if-eqz v2, :cond_2
    iget-object v2, p1, Lde/robv/android/xposed/XC_MethodHook$MethodHookParam;->args:[Ljava/lang/Object;
    const/4 v3, 0x0
    aget-object v2, v2, v3
    check-cast v2, [B
    iget-object v3, p1, Lde/robv/android/xposed/XC_MethodHook$MethodHookParam;->result:Ljava/lang/Object;
    check-cast v3, [B
    if-eqz v2, :cond_0
    array-length v4, v2
    const/16 v5, 0x1000
    if-gt v4, v5, :cond_2
    :cond_0
    if-eqz v3, :cond_1
    array-length v4, v3
    const/16 v5, 0x1000
    if-gt v4, v5, :cond_2
    :cond_1
    invoke-static {v2}, Lcom/xcoder/haydayhook/LogUtil;->b64([B)Ljava/lang/String;
    move-result-object v2
    invoke-static {v3}, Lcom/xcoder/haydayhook/LogUtil;->b64([B)Ljava/lang/String;
    move-result-object v3
    new-instance v4, Ljava/lang/StringBuilder;
    invoke-direct {v4}, Ljava/lang/StringBuilder;-><init>()V
    const-string v5, "Cipher.doFinal alg="
    invoke-virtual {v4, v5}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v4, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    const-string v1, " inB64="
    invoke-virtual {v4, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v4, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    const-string v1, " outB64="
    invoke-virtual {v4, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v4, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invoke-virtual {v4}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
    move-result-object v1
    invoke-static {v1}, Lcom/xcoder/haydayhook/LogUtil;->log(Ljava/lang/String;)V
    :cond_2
    :try_end_0
    .catch Ljava/lang/Throwable; {:try_start_0 .. :try_end_0} :catch_0
    return-void
    :catch_0
    move-exception v0
    invoke-static {v0}, Lde/robv/android/xposed/XposedBridge;->log(Ljava/lang/Throwable;)V
    return-void
.end method
