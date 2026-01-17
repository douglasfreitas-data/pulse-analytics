import { createClient } from '@supabase/supabase-js';

const supabaseUrl = 'https://pthfxmypcxqjfstqwokf.supabase.co';
const supabaseKey = 'sb_publishable_V4ZrfeZNld9VROJOWZcE_w_N93BHvqd';

export const supabase = createClient(supabaseUrl, supabaseKey);
