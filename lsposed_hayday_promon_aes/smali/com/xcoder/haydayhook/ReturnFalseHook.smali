.class public Lcom/xcoder/haydayhook/ReturnFalseHook;
.super Lde/robv/android/xposed/XC_MethodHook;

.method public constructor <init>()V
    .locals 0
    invoke-direct {p0}, Lde/robv/android/xposed/XC_MethodHook;-><init>()V
    return-void
.end method

.method protected beforeHookedMethod(Lde/robv/android/xposed/XC_MethodHook$MethodHookParam;)V
    .locals 1
    sget-object v0, Ljava/lang/Boolean;->FALSE:Ljava/lang/Boolean;
    invoke-virtual {p1, v0}, Lde/robv/android/xposed/XC_MethodHook$MethodHookParam;->setResult(Ljava/lang/Object;)V
    const-string v0, "HayDayHook returned false"
    invoke-static {v0}, Lcom/xcoder/haydayhook/LogUtil;->log(Ljava/lang/String;)V
    return-void
.end method
