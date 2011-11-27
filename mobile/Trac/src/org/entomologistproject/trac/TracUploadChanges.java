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

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import org.xmlrpc.android.XMLRPCException;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

// This uploads all changes to the fields and text values (like the summary)

public class TracUploadChanges extends TracBackend {
	private Bundle changelog;
	public TracUploadChanges(Context context,
							 String url,
							 String username,
							 String password,
							 Bundle changelog) {
		super(context, url, username, password);
		this.changelog = changelog;
	}
	
	@SuppressWarnings("unchecked")
	public void uploadChanges() throws EntomologistException
	{
		// We need to remove the bug_id and tracker_id otherwise
		// Trac will get confused when we update the bug
		long bugId = changelog.getLong("bug_id");
		changelog.remove("bug_id");
		long trackerId = changelog.getLong("tracker_id");
		changelog.remove("tracker_id");
		try {
			Object[] bugValues = (Object[]) client.call("ticket.get", bugId);
			if (bugValues.length != 4)
			{
				//Log.v("Entomologist", "Length: " + bugValues.length);
				throw new EntomologistException("This bug seems to suddenly be invalid!");
			}
			
			Map<String, Object> bugMap = (Map<String, Object>) bugValues[3];
			String ts = (String) bugMap.get("_ts");

			Map<String, Object> newMap = new HashMap<String, Object>();
			Set<String> keys = changelog.keySet();  
			Iterator<String> iterate = keys.iterator();
			while (iterate.hasNext()) {
			    String key = iterate.next();
			    //Log.v("Entomologist", "Setting " + key + " to " + changelog.getString(key));
			    newMap.put(key, changelog.getString(key));
			}
			
			newMap.put("_ts", ts);
			newMap.put("action", "leave");
			if (changelog.containsKey("severity"))
				newMap.put("type", changelog.getString("severity"));
			if (changelog.containsKey("priority"))
				newMap.put("priority", changelog.getString("priority"));
			if (changelog.containsKey("status"))
			{
				String status = changelog.getString("status");
				newMap.put("status", status);
				if (status.equals("accepted"))
				{
					newMap.put("action", "accept");
				}
				else if (status.equals("assigned"))
				{
					newMap.remove("action");
					newMap.put("owner", username);
				}
				else if (status.equals("closed"))
				{
					// For some reason, if the action is set to "resolve",
					// Trac ignores the 'resolution' field.
					newMap.remove("action");
				}
			}

			if (changelog.containsKey("owner"))
			{
				newMap.put("status", "assigned");
				newMap.remove("action");
				newMap.put("owner", changelog.getString("owner"));
			}
			
			Object[] result = (Object[]) client.call("ticket.update", bugId,
										  "", // No comment
										  newMap,
										  false,
										  username);
			if (result.length != 4)
			{
				//Log.v("Entomologist", "Length: " + bugValues.length);
				throw new EntomologistException("This bug seems to suddenly be invalid!");
			}
			Date modified = (Date) result[2];
			SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss+0000");
			String bugLastModified = sdf.format(modified);
			
			Map<String, Object> updatedBugMap = (Map<String, Object>) result[3];
			updatedBugMap.put("last_modified", bugLastModified);
			if (updatedBugMap.get("owner").equals(username))
				updatedBugMap.put("bug_type", "ASSIGNED");
			else if (updatedBugMap.get("reporter").equals(username))
				updatedBugMap.put("bug_type", "REPORTED");
			else
				updatedBugMap.put("bug_type", "CC");
			
			dbHelper.deleteBug(trackerId, bugId);
			if (!updatedBugMap.get("status").equals("closed"))
				dbHelper.insertBug(trackerId, bugId, updatedBugMap);
		}
		catch (XMLRPCException e)
		{
			e.printStackTrace();
			throw new EntomologistException(e.getMessage());
		}
		
	}
	@Override
	protected Void doInBackground(Handler... params) {
		Handler h = params[0];
		Message m = h.obtainMessage();
		Bundle b = new Bundle();
		b.putBoolean("error", false);
		
		try
		{
			uploadChanges();
			b.putBoolean("changes_uploaded", true);
		}
		catch (EntomologistException e)
		{
			//Log.v("Entomologist", "EntomologistException caught: " + e.getMessage());
			b.putBoolean("error", true);
			b.putString("errormsg", e.getMessage());
		}

		m.setData(b);
		m.sendToTarget();
		dbHelper.close();

		return null;
	}
}
