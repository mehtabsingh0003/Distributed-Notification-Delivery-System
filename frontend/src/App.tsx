import { FormEvent, useEffect, useState } from "react";
import {
  Activity,
  Bell,
  CheckCircle2,
  ChevronRight,
  Clock3,
  LayoutDashboard,
  Mail,
  MessageSquare,
  Plus,
  RefreshCw,
  Send,
  Settings2,
  Smartphone,
  X,
} from "lucide-react";

type Notification = {
  id: string;
  idempotencyKey: string;
  channel: "EMAIL" | "SMS" | "PUSH";
  priority: "LOW" | "NORMAL" | "HIGH";
  recipient: string;
  subject: string;
  body?: string;
  status: string;
  attemptCount: number;
};
type Attempt = {
  attemptNumber: number;
  status: string;
  providerRef: string | null;
  errorMessage: string | null;
  attemptedAt?: string;
};
type NotificationDetail = Notification & { deliveryAttempts: Attempt[] };

const channelIcons = { EMAIL: Mail, SMS: MessageSquare, PUSH: Smartphone };

async function api<T>(path: string, options?: RequestInit): Promise<T> {
  const response = await fetch(path, {
    headers: { "Content-Type": "application/json" },
    ...options,
  });
  const data = await response.json();
  if (!response.ok) throw new Error(data.error || "Request failed");
  return data;
}

function App() {
  const [notifications, setNotifications] = useState<Notification[]>([]);
  const [selected, setSelected] = useState<NotificationDetail | null>(null);
  const [health, setHealth] = useState<"online" | "offline">("offline");
  const [showComposer, setShowComposer] = useState(false);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [lastUpdated, setLastUpdated] = useState<Date | null>(null);
  const [sending, setSending] = useState(false);
  const [submitError, setSubmitError] = useState("");
  const [submitSuccess, setSubmitSuccess] = useState(false);

  const loadNotifications = async () => {
    setLoading(true);
    try {
      const data = await api<{ notifications: Notification[] }>(
        "/api/notifications",
      );
      setNotifications(data.notifications);
      setLastUpdated(new Date());
      setError("");
    } catch (err) {
      setHealth("offline");
      setError(err instanceof Error ? err.message : "Unable to reach API");
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    const checkHealth = () => {
      void api<{ status: string }>("/healthz")
        .then(() => setHealth("online"))
        .catch(() => setHealth("offline"));
    };
    void loadNotifications();
    checkHealth();
    const healthTimer = window.setInterval(checkHealth, 15000);
    return () => window.clearInterval(healthTimer);
  }, []);

  useEffect(() => {
    if (!autoRefresh) return;
    const refreshTimer = window.setInterval(() => {
      void loadNotifications();
    }, 5000);
    return () => window.clearInterval(refreshTimer);
  }, [autoRefresh]);

  const openDetail = async (id: string) => {
    setSending(true);
    setSubmitError("");
    setSubmitSuccess(false);
    try {
      setSelected(await api<NotificationDetail>(`/api/notifications/${id}`));
    } catch (err) {
      setError(
        err instanceof Error ? err.message : "Unable to load notification",
      );
    }
  };

  const submit = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    const formElement = event.currentTarget;
    const form = new FormData(formElement);
    try {
      await api("/api/notifications", {
        method: "POST",
        body: JSON.stringify({
          channel: form.get("channel"),
          priority: form.get("priority"),
          recipient: form.get("recipient"),
          subject: form.get("subject"),
          body: form.get("body"),
          idempotencyKey: crypto.randomUUID(),
        }),
      });
      formElement.reset();
      await loadNotifications();
      setSubmitSuccess(true);
      window.setTimeout(() => setShowComposer(false), 900);
    } catch (err) {
      setSubmitError(
        err instanceof Error ? err.message : "Unable to send notification",
      );
    } finally {
      setSending(false);
    }
  };

  const pendingCount = notifications.filter(
    (notification) => notification.status === "PENDING",
  ).length;
  const processingCount = notifications.filter(
    (notification) => notification.status === "PROCESSING",
  ).length;
  const sentCount = notifications.filter(
    (notification) => notification.status === "SENT",
  ).length;
  const failedCount = notifications.filter(
    (notification) => notification.status === "FAILED",
  ).length;
  const deadLetteredCount = notifications.filter(
    (notification) => notification.status === "DEAD_LETTERED",
  ).length;
  const statusStyles: Record<string, string> = {
    SENT: "bg-emerald-50 text-emerald-700",
    SUCCESS: "bg-emerald-50 text-emerald-700",
    PENDING: "bg-amber-50 text-amber-700",
    FAILED: "bg-rose-50 text-rose-700",
    TRANSIENT_FAILURE: "bg-amber-50 text-amber-700",
    PERMANENT_FAILURE: "bg-rose-50 text-rose-700",
    PROCESSING: "bg-sky-50 text-sky-700",
    DEAD_LETTERED: "bg-violet-50 text-violet-700",
  };

  return (
    <div className="min-h-screen bg-[#f4f7f5] text-[#182a25] lg:flex">
      <aside className="flex h-16 items-center justify-between bg-[#102a24] px-5 text-white lg:fixed lg:inset-y-0 lg:flex lg:h-screen lg:w-64 lg:flex-col lg:items-stretch lg:justify-start lg:px-4 lg:py-6">
        <div className="flex items-center gap-3 px-2">
          <span className="grid size-9 place-items-center rounded-xl bg-[#b6e9c9] text-[#102a24]">
            <Activity size={19} />
          </span>
          <span className="text-lg font-extrabold tracking-tight">
            signal<span className="text-[#8bd6a5]">desk</span>
          </span>
        </div>
        <div className="mt-14 hidden text-[10px] font-medium uppercase tracking-[0.18em] text-[#789188] lg:block">
          Operations console
        </div>
        <nav className="hidden space-y-2 lg:mt-4 lg:block">
          <button className="flex w-full items-center gap-3 rounded-xl bg-[#24483d] px-3 py-3 text-left text-sm font-bold text-white">
            <LayoutDashboard size={18} /> Overview{" "}
            <span className="ml-auto rounded-full bg-[#a9dfba] px-2 py-0.5 text-[11px] text-[#102a24]">
              {notifications.length}
            </span>
          </button>
          <button className="flex w-full items-center gap-3 rounded-xl px-3 py-3 text-left text-sm font-semibold text-[#9fb7aa]">
            <Settings2 size={18} /> Settings
          </button>
        </nav>
        <div className="hidden items-center gap-2 border-t border-[#2c4c42] px-2 pt-4 text-[11px] text-[#a8bbb1] lg:mt-auto lg:flex">
          <span
            className={`size-2 rounded-full ${health === "online" ? "bg-[#70d898] shadow-[0_0_0_4px_#70d89820]" : "bg-[#d26942]"}`}
          />{" "}
          API {health === "online" ? "connected" : "unavailable"}
          <span className="ml-auto font-mono text-[#71877d]">:8080</span>
        </div>
        <div className="flex items-center gap-2 text-xs font-semibold text-[#a8bbb1] lg:hidden">
          <span
            className={`size-2 rounded-full ${health === "online" ? "bg-[#70d898]" : "bg-[#d26942]"}`}
          />{" "}
          {health === "online" ? "Online" : "Offline"}
        </div>
      </aside>
      <main className="w-full lg:ml-64">
        <div className="mx-auto max-w-[1440px] px-4 py-6 sm:px-8 sm:py-8 xl:px-10">
          <header className="mb-8 flex flex-col gap-5 sm:flex-row sm:items-end sm:justify-between">
            <div>
              <p className="mb-2 font-mono text-[10px] uppercase tracking-[0.16em] text-[#86968f]">
                OPERATIONS CONSOLE / LIVE DELIVERY
              </p>
              <h1 className="text-3xl font-extrabold tracking-[-0.04em] text-[#142a24] sm:text-4xl">
                Delivery overview
              </h1>
              <p className="mt-2 max-w-lg text-sm leading-6 text-[#71817a]">
                A live pulse on every message moving through your notification
                infrastructure.
              </p>
            </div>
            <div className="flex gap-2">
              <button
                className="grid min-h-11 min-w-11 place-items-center rounded-xl border border-[#dfe8e2] bg-white text-[#536b60] shadow-sm transition hover:-translate-y-0.5 hover:border-[#96c5a5]"
                aria-label="Refresh notifications"
                title="Refresh notifications"
                onClick={() => void loadNotifications()}
              >
                <RefreshCw size={18} />
              </button>
              <button className="flex min-h-11 flex-1 items-center justify-center gap-2 rounded-xl bg-[#12382d] px-4 py-3 text-sm font-bold text-white shadow-[0_8px_20px_#12382d20] transition hover:-translate-y-0.5 hover:bg-[#1d503f] sm:flex-none" onClick={() => { setSubmitError(""); setSubmitSuccess(false); setShowComposer(true); }}>
                <Plus size={18} />
                <span>New notification</span>
              </button>
            </div>
          </header>
          {error && (
            <div className="mb-5 flex items-center justify-between gap-3 rounded-xl border border-rose-200 bg-rose-50 px-4 py-3 text-sm text-rose-700">
              <span><strong>Unable to connect to notification service.</strong> {error}</span>
              <button className="flex min-h-11 min-w-11 items-center justify-center" aria-label="Dismiss error" onClick={() => setError("")}>
                <X size={16} />
              </button>
            </div>
          )}
          <section className="mb-6 flex flex-wrap items-center gap-x-5 gap-y-3 rounded-2xl border border-[#dfe8e2] bg-[#eaf5ec] px-4 py-3 text-xs text-[#49675a] sm:px-5">
            <div className={`flex items-center gap-2 font-bold ${health === "online" ? "text-[#27764a]" : "text-[#b34e32]"}`}>
              <span className={`size-2 rounded-full ${health === "online" ? "bg-[#43aa6c] shadow-[0_0_0_4px_#43aa6c20]" : "bg-[#d26942]"}`} />
              {health === "online" ? "System operational" : "System unavailable"}
            </div>
            <span className="h-4 w-px bg-[#c8dfce]" aria-hidden="true" />
            <span>API: <strong>{health === "online" ? "Connected" : "Unavailable"}</strong></span>
            <span>RabbitMQ: <strong>Not exposed</strong></span>
            <span>Database: <strong>Not exposed</strong></span>
            <span className="ml-auto flex items-center gap-2">
              {lastUpdated ? `Updated ${lastUpdated.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })}` : "Waiting for update"}
              <button
                className="min-h-9 rounded-lg border border-[#bfd8c5] bg-white px-3 font-bold text-[#397352] transition hover:border-[#83b994]"
                aria-pressed={autoRefresh}
                onClick={() => setAutoRefresh((enabled) => !enabled)}
              >
                Auto refresh: {autoRefresh ? "ON" : "OFF"}
              </button>
            </span>
          </section>
          <section className="mb-6 grid grid-cols-1 gap-3 min-[520px]:grid-cols-2 md:grid-cols-3 xl:grid-cols-6">
            <div className="rounded-2xl border border-[#dfe8e2] bg-white p-5 shadow-[0_14px_40px_#1e3e3010]">
              <div className="mb-5 flex items-center justify-between">
                <span className="grid size-10 place-items-center rounded-xl bg-[#e4f6e9] text-[#23824f]">
                  <CheckCircle2 size={19} />
                </span>
                <span className="font-mono text-[10px] text-[#91a19a]">
                  01 / 06
                </span>
              </div>
              <p className="text-xs font-semibold text-[#7d8b84]">
                Total notifications
              </p>
              <strong className="mt-1 block text-3xl tracking-[-0.05em]">
                {notifications.length}
              </strong>
              <p className="mt-2 text-xs text-[#4b9a68]">
                {sentCount} delivered successfully
              </p>
            </div>
            <div className="rounded-2xl border border-[#dfe8e2] bg-white p-5 shadow-[0_14px_40px_#1e3e3010]">
              <div className="mb-5 flex items-center justify-between">
                <span className="grid size-10 place-items-center rounded-xl bg-[#fff2dc] text-[#b46c1f]">
                  <Clock3 size={19} />
                </span>
                <span className="font-mono text-[10px] text-[#91a19a]">
                  02 / 06
                </span>
              </div>
              <p className="text-xs font-semibold text-[#7d8b84]">
                Pending
              </p>
              <strong className="mt-1 block text-3xl tracking-[-0.05em]">
                {pendingCount}
              </strong>
              <p className="mt-2 text-xs text-[#b46c1f]">
                {failedCount} failed attempts
              </p>
            </div>
            <div className="rounded-2xl border border-[#dfe8e2] bg-[#12382d] p-5 text-white shadow-[0_14px_40px_#1e3e3020]">
              <div className="mb-5 flex items-center justify-between">
                <span className="grid size-10 place-items-center rounded-xl bg-[#2a5949] text-[#a9e7bc]">
                  <Activity size={19} />
                </span>
                <span className="font-mono text-[10px] text-[#91b9a6]">
                  03 / 06
                </span>
              </div>
              <p className="text-xs font-semibold text-[#a6c4b5]">Processing</p>
              <strong className="mt-1 block text-3xl tracking-[-0.05em]">
                {processingCount}
              </strong>
              <p className="mt-2 text-xs text-[#a9e7bc]">Active workers</p>
            </div>
            {[
              ["Sent", sentCount, "bg-emerald-50", "text-emerald-700"],
              ["Failed", failedCount, "bg-rose-50", "text-rose-700"],
              ["Dead-lettered", deadLetteredCount, "bg-violet-50", "text-violet-700"],
            ].map(([label, count, background, color], index) => (
              <div className="rounded-2xl border border-[#dfe8e2] bg-white p-5 shadow-[0_14px_40px_#1e3e3010]" key={label}>
                <div className="mb-5 flex items-center justify-between">
                  <span className={`grid size-10 place-items-center rounded-xl ${background} ${color}`}>
                    <CheckCircle2 size={19} />
                  </span>
                  <span className="font-mono text-[10px] text-[#91a19a]">0{index + 4} / 06</span>
                </div>
                <p className="text-xs font-semibold text-[#7d8b84]">{label}</p>
                <strong className="mt-1 block text-3xl tracking-[-0.05em]">{count}</strong>
                <p className={`mt-2 text-xs ${color}`}>{label === "Sent" ? "Delivered successfully" : label === "Failed" ? "Needs attention" : "Parked for review"}</p>
              </div>
            ))}
          </section>
          <section className="overflow-hidden rounded-2xl border border-[#dfe8e2] bg-white shadow-[0_14px_40px_#1e3e3010]">
            <div className="flex flex-col gap-3 border-b border-[#e8efea] px-5 py-5 sm:flex-row sm:items-center sm:justify-between sm:px-6">
              <div>
                <div className="flex items-center gap-3">
                  <h2 className="text-lg font-extrabold tracking-tight">
                    Recent notifications
                  </h2>
                  <span className="flex items-center gap-1.5 font-mono text-[10px] text-[#3a9160]">
                    <span className="size-1.5 rounded-full bg-[#43aa6c] shadow-[0_0_0_4px_#43aa6c20]" />{" "}
                    LIVE
                  </span>
                </div>
                <p className="mt-1 text-xs text-[#819089]">
                  Latest delivery events across every channel
                </p>
              </div>
              <span className="w-fit rounded-lg bg-[#f1f6f2] px-3 py-2 font-mono text-[10px] text-[#789188]">
                50 EVENT RETENTION
              </span>
            </div>
            {loading ? (
              <div className="space-y-3 p-5" aria-label="Loading delivery events">
                {[1, 2, 3].map((item) => <div className="h-16 animate-pulse rounded-xl bg-[#f1f6f2]" key={item} />)}
              </div>
            ) : notifications.length === 0 ? (
              <div className="px-6 py-14 text-center">
                <Bell className="mx-auto mb-3 text-[#6fa17e]" size={25} />
                <strong className="block text-sm">No notifications yet</strong>
                <p className="mt-1 text-sm text-[#77857d]">Send your first notification to see delivery activity.</p>
              </div>
            ) : (
              <div className="divide-y divide-[#edf1ee]">
                {notifications.map((notification) => {
                  const Icon = channelIcons[notification.channel] || Mail;
                  return (
                    <button
                      className="group grid w-full grid-cols-[minmax(0,1fr)_auto] gap-x-4 gap-y-2 px-5 py-4 text-left transition hover:bg-[#f7fbf8] sm:grid-cols-[minmax(220px,1.6fr)_minmax(110px,0.8fr)_90px_120px_60px_20px] sm:items-center sm:px-6"
                      key={notification.id}
                      onClick={() => void openDetail(notification.id)}
                    >
                      <span className="flex min-w-0 items-center gap-3">
                        <span className="grid size-9 shrink-0 place-items-center rounded-xl bg-[#e7f4ea] text-xs font-extrabold text-[#37865a]">
                          {notification.recipient[0]?.toUpperCase()}
                        </span>
                        <span className="min-w-0">
                          <strong className="block truncate text-sm font-bold text-[#263a33]">
                            {notification.recipient}
                          </strong>
                          <small className="mt-1 block truncate text-xs text-[#8b9892]">
                            {notification.subject || "No subject"}
                          </small>
                          <small className="mt-1 block truncate font-mono text-[10px] text-[#a0aca6]">
                            ID {notification.id}
                          </small>
                        </span>
                      </span>
                      <span className="flex items-center justify-end gap-2 text-[10px] font-medium text-[#52635b] sm:justify-start">
                        <Icon size={15} />
                        {notification.channel}
                      </span>
                      <span
                        className={`w-fit rounded-md px-2 py-1 font-mono text-[10px] ${notification.priority === "HIGH" ? "bg-rose-50 text-rose-700" : notification.priority === "LOW" ? "bg-violet-50 text-violet-700" : "bg-[#edf6ef] text-[#54715e]"}`}
                      >
                        {notification.priority}
                      </span>
                      <span
                        className={`flex w-fit items-center gap-1.5 rounded-md px-2 py-1 font-mono text-[10px] ${statusStyles[notification.status] || "bg-slate-50 text-slate-600"}`}
                      >
                        <span className="size-1.5 rounded-full bg-current" />
                        {notification.status}
                      </span>
                      <span className="hidden text-xs text-[#63716b] sm:block">
                        {notification.attemptCount}
                      </span>
                      <span className="col-start-1 text-xs text-[#77857d] sm:hidden">
                        {notification.priority} <span aria-hidden="true">•</span> {notification.attemptCount} {notification.attemptCount === 1 ? "attempt" : "attempts"}
                      </span>
                      <ChevronRight
                        size={17}
                        className="text-[#a5b0aa] transition group-hover:translate-x-1 group-hover:text-[#3d7559]"
                      />
                    </button>
                  );
                })}
              </div>
            )}
          </section>
        </div>
      </main>
      {showComposer && (
        <div
          className="fixed inset-0 z-10 grid place-items-center bg-[#10231fcc] p-4 backdrop-blur-sm"
          onMouseDown={(event) => {
            if (event.target === event.currentTarget) setShowComposer(false);
          }}
        >
          <form
            className="max-h-[calc(100vh-2rem)] w-full max-w-xl overflow-y-auto rounded-2xl border border-[#e1ebe2] bg-white p-6 shadow-2xl sm:p-8"
            onSubmit={submit}
          >
            <div className="mb-7 flex items-start justify-between">
              <div>
                <p className="mb-2 font-mono text-[10px] uppercase tracking-[0.16em] text-[#819089]">
                  Outbound message
                </p>
                <h2 className="text-2xl font-extrabold tracking-tight">
                  New notification
                </h2>
              </div>
              <button
                type="button"
                className="grid min-h-11 min-w-11 place-items-center rounded-xl border border-[#dfe8e2] text-[#536b60]"
                aria-label="Close new notification form"
                onClick={() => setShowComposer(false)}
              >
                <X size={18} />
              </button>
            </div>
            <label className="mb-4 block text-xs font-bold text-[#53635b]">
              Channel
              <select
                className="mt-2 block w-full rounded-lg border border-[#dce5de] bg-[#fbfcfb] px-3 py-3 text-sm outline-none focus:border-[#77b88b]"
                name="channel"
                defaultValue="EMAIL"
              >
                <option>EMAIL</option>
                <option>SMS</option>
                <option>PUSH</option>
              </select>
            </label>
            <div className="grid gap-4 sm:grid-cols-[120px_1fr]">
              <label className="mb-4 block text-xs font-bold text-[#53635b]">
                Priority
                <select
                  className="mt-2 block w-full rounded-lg border border-[#dce5de] bg-[#fbfcfb] px-3 py-3 text-sm outline-none focus:border-[#77b88b]"
                  name="priority"
                  defaultValue="HIGH"
                >
                  <option>LOW</option>
                  <option>NORMAL</option>
                  <option>HIGH</option>
                </select>
              </label>
              <label className="mb-4 block text-xs font-bold text-[#53635b]">
                Recipient
                <input
                  className="mt-2 block w-full rounded-lg border border-[#dce5de] bg-[#fbfcfb] px-3 py-3 text-sm outline-none focus:border-[#77b88b]"
                  required
                  name="recipient"
                  placeholder="user@example.com"
                />
              </label>
            </div>
            <label className="mb-4 block text-xs font-bold text-[#53635b]">
              Subject
              <input
                className="mt-2 block w-full rounded-lg border border-[#dce5de] bg-[#fbfcfb] px-3 py-3 text-sm outline-none focus:border-[#77b88b]"
                name="subject"
                placeholder="Your notification subject"
              />
            </label>
            <label className="mb-4 block text-xs font-bold text-[#53635b]">
              Message
              <textarea
                className="mt-2 block w-full resize-y rounded-lg border border-[#dce5de] bg-[#fbfcfb] px-3 py-3 text-sm outline-none focus:border-[#77b88b]"
                required
                name="body"
                rows={5}
                placeholder="Write the message to deliver..."
              />
            </label>
            {submitError && <p className="mb-4 rounded-lg border border-rose-200 bg-rose-50 px-3 py-2 text-sm text-rose-700" role="alert">{submitError}</p>}
            {submitSuccess && <p className="mb-4 rounded-lg border border-emerald-200 bg-emerald-50 px-3 py-2 text-sm text-emerald-700" role="status">Notification accepted and queued.</p>}
            <button disabled={sending || submitSuccess} className="flex min-h-11 w-full items-center justify-center gap-2 rounded-xl bg-[#12382d] px-4 py-3 text-sm font-bold text-white transition hover:bg-[#1d503f] disabled:cursor-not-allowed disabled:opacity-60">
              <Send size={17} /> {sending ? "Sending..." : submitSuccess ? "Queued" : "Send notification"}
            </button>
          </form>
        </div>
      )}
      {selected && (
        <div
          className="fixed inset-0 z-10 grid place-items-center bg-[#10231fcc] p-4 backdrop-blur-sm"
          onMouseDown={(event) => {
            if (event.target === event.currentTarget) setSelected(null);
          }}
        >
          <section className="max-h-[calc(100vh-2rem)] w-full max-w-xl overflow-y-auto rounded-2xl border border-[#e1ebe2] bg-white p-6 shadow-2xl sm:p-8">
            <div className="mb-7 flex items-start justify-between">
              <div>
                <p className="mb-2 font-mono text-[10px] uppercase tracking-[0.16em] text-[#819089]">
                  Delivery record
                </p>
                <h2 className="text-2xl font-extrabold tracking-tight">
                  {selected.subject || "Notification detail"}
                </h2>
              </div>
              <button
                className="grid min-h-11 min-w-11 place-items-center rounded-xl border border-[#dfe8e2] text-[#536b60]"
                aria-label="Close notification details"
                onClick={() => setSelected(null)}
              >
                <X size={18} />
              </button>
            </div>
            <div className="mb-6 grid gap-4 rounded-xl border border-[#e1ebe2] bg-[#f4f8f4] p-4 sm:grid-cols-2 lg:grid-cols-3">
              <div className="sm:col-span-2 lg:col-span-3"><p className="text-[10px] uppercase tracking-wider text-[#819089]">Notification ID</p><strong className="mt-1 block break-all font-mono text-xs">{selected.id}</strong></div>
              <div><p className="text-[10px] uppercase tracking-wider text-[#819089]">Status</p><span className={`mt-1 inline-flex rounded-md px-2 py-1 font-mono text-[10px] ${statusStyles[selected.status] || "bg-slate-50 text-slate-600"}`}>{selected.status}</span></div>
              <div><p className="text-[10px] uppercase tracking-wider text-[#819089]">Channel</p><strong className="mt-1 block text-sm">{selected.channel}</strong></div>
              <div><p className="text-[10px] uppercase tracking-wider text-[#819089]">Recipient</p><strong className="mt-1 block truncate text-sm">{selected.recipient}</strong></div>
              <div><p className="text-[10px] uppercase tracking-wider text-[#819089]">Priority</p><strong className="mt-1 block text-sm">{selected.priority}</strong></div>
              <div><p className="text-[10px] uppercase tracking-wider text-[#819089]">Attempts</p><strong className="mt-1 block text-sm">{selected.attemptCount}</strong></div>
            </div>
            <h3 className="mb-3 text-sm font-extrabold">Delivery History</h3>
            {selected.deliveryAttempts.length === 0 ? (
              <p className="text-sm text-[#77857d]">No attempts recorded.</p>
            ) : (
              selected.deliveryAttempts.map((attempt) => (
                <div
                  className="flex items-center gap-3 border-t border-[#e1ebe2] py-3"
                  key={attempt.attemptNumber}
                >
                  <span className="grid size-7 shrink-0 place-items-center rounded-full bg-[#edf4ee] font-mono text-[11px] text-[#397352]">
                    {attempt.attemptNumber}
                  </span>
                  <div className="min-w-0 flex-1">
                    <strong className="block text-sm">
                      Attempt {attempt.attemptNumber}
                    </strong>
                    <small className="mt-1 block truncate text-xs text-[#77857d]">
                      {attempt.providerRef ||
                        attempt.errorMessage ||
                        "Provider response recorded"}
                    </small>
                    {attempt.attemptedAt && <small className="mt-1 block text-[10px] text-[#9aa8a1]">{attempt.attemptedAt}</small>}
                  </div>
                  <span
                    className={`rounded-md px-2 py-1 font-mono text-[10px] ${statusStyles[attempt.status] || "bg-slate-50 text-slate-600"}`}
                  >
                    {attempt.status}
                  </span>
                </div>
              ))
            )}
          </section>
        </div>
      )}
    </div>
  );
}

export default App;
