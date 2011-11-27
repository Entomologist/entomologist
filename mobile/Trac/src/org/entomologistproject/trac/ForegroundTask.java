/*
 *  Entomologist Mobile/Trac
 *  Copyright (C) 2011 Matt Barringer
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 *  Author: Matt Barringer <matt@entomologist-project.org>
 */
package org.entomologistproject.trac;

import java.util.List;

import android.app.ActivityManager;
import android.app.ActivityManager.RunningAppProcessInfo;
import android.content.Context;
import android.os.AsyncTask;

// This task is just used to test whether or not we're in the foreground.
// Apparently checking RunningAppProcessInfo.IMPORTANCE_FOREGROUND will *always*
// return true if it's on the activity's UI thread, so doing it as a task
// works around that.
public class ForegroundTask extends AsyncTask<Context, Void, Boolean> {
	@Override
	protected Boolean doInBackground(Context... params) {
		final Context context = params[0].getApplicationContext();
		final String name = context.getPackageName();

		ActivityManager manager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
		List<RunningAppProcessInfo> procs = manager.getRunningAppProcesses();
		if (procs == null)
			return false;
		
		for (RunningAppProcessInfo proc : procs)
		{
			if (proc.importance == RunningAppProcessInfo.IMPORTANCE_FOREGROUND
					&& proc.processName.equals(name))
				return true;
		}
		
		return false;
	}
}
