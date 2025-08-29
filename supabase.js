// supabase/functions/esp32-webhook/index.ts
import { serve } from "https://deno.land/std@0.224.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

serve(async (req) => {
  try {
    if (req.method !== "POST") {
      return new Response("Method Not Allowed", { status: 405 });
    }

    const { device_token, event_type, payload } = await req.json();

    if (!device_token || !event_type) {
      return new Response(JSON.stringify({ error: "Missing fields" }), { status: 400 });
    }

    // Connect to DB as service
    const supabaseUrl = Deno.env.get("SUPABASE_URL");
    const serviceKey = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY");
    const supabase = createClient(supabaseUrl, serviceKey);

    // Validate device
    const { data: device, error: devErr } = await supabase
      .from("devices")
      .select("id")
      .eq("token", device_token)
      .single();

    if (devErr || !device) {
      return new Response(JSON.stringify({ error: "Invalid device token" }), { status: 401 });
    }

    // Log event
    const { error: evtErr } = await supabase.from("events").insert({
      device_id: device.id,
      event_type,
      payload: payload ?? {},
    });

    if (evtErr) {
      return new Response(JSON.stringify({ error: evtErr.message }), { status: 500 });
    }

    // If mode change, log in mode_logs
    if (event_type === "mode_changed" && payload?.mode) {
      await supabase.from("mode_logs").insert({
        device_id: device.id,
        mode: payload.mode,
        created_by: "device",
      });
    }

    return new Response(JSON.stringify({ ok: true }), { status: 200 });
  } catch (e) {
    return new Response(JSON.stringify({ error: e.message }), { status: 500 });
  }
});




//-----------------------------------------------------------------------

import { serve } from "https://deno.land/std/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js";

const supabase = createClient(
  Deno.env.get("SUPABASE_URL"),
  Deno.env.get("SUPABASE_ANON_KEY")
);

serve(async (req) => {
  const { user_id, sensor_type, value } = await req.json();

  // insert reading
  const { error } = await supabase
    .from("sensor_readings")
    .insert([{ user_id, sensor_type, value }]);

  if (error) return new Response(JSON.stringify({ error: error.message }), { status: 400 });

  // مثال: check security mode على أساس alerts بدل device
  if (sensor_type === "motion" && value > 0) {
    await supabase.from("alerts").insert([{
      user_id,
      message: "Motion detected while security mode is ON",
      level: "critical"
    }]);
  }

  return new Response(JSON.stringify({ success: true }), { status: 200 });
});





//------------------------------------------------------------

import { serve } from "https://deno.land/std/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js";

const supabase2 = createClient(
  Deno.env.get("SUPABASE_URL"),
  Deno.env.get("SUPABASE_SERVICE_ROLE_KEY") // مهم عشان يقدر يكتب
);

serve(async (req) => {
  const { user_id, message, level } = await req.json();

  const { error } = await supabase
    .from("alerts")
    .insert([{ user_id, message, level }]);

  if (error) return new Response(JSON.stringify({ error: error.message }), { status: 400 });

  return new Response(JSON.stringify({ success: true }), { status: 200 });
});





//_----------------------------------------------------------



import { serve } from "https://deno.land/std/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js";

const supabase3 = createClient(
  Deno.env.get("SUPABASE_URL"),
  Deno.env.get("SUPABASE_ANON_KEY")
);

serve(async (req) => {
  const { user_id, action, status } = await req.json();

  // سجل الدخول/الخروج
  await supabase.from("access_logs").insert([{ user_id, action, status }]);

  // check failed attempts خلال آخر 5 دقايق
  const { count } = await supabase
    .from("access_logs")
    .select("*", { count: "exact", head: true })
    .eq("user_id", user_id)
    .eq("status", "failed")
    .gte("created_at", new Date(Date.now() - 5 * 60 * 1000).toISOString());

  if ((count ?? 0) >= 3) {
    await supabase.from("alerts").insert([{
      user_id,
      message: "3 failed login attempts – Security alert!",
      level: "critical"
    }]);
  }

  return new Response(JSON.stringify({ success: true }), { status: 200 });
});