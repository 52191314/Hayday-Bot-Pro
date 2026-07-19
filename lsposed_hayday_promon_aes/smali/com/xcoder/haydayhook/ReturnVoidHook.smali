.class public Lcom/xcoder/haydayhook/ReturnVoidHook;
.super Lde/robv/android/xposed/XC_MethodHook;

.method public constructor <init>()V
    .locals 0
    invoke-direct {p0}, Lde/robv/android/xposed/XC_MethodHook;-><init>()V
    return-void
.end method

.method protected beforeHookedMethod(Lde/robv/android/xposed/XC_MethodHook$MethodHookParam;)V
    .locals 1
    const-string v0, "HayDayHook bypassed void method"
    invoke-static {v0}, Lcom/xcoder/haydayhook/LogUtil;->log(Ljava/lang/String;)V
    const/4 v0, 0x0
    invoke-virtual {p1, v0}, Lde/robv/android/xposed/XC_MethodHook$MethodHookParam;->setResult(Ljava/lang/Object;)V
    return-void
.end method
