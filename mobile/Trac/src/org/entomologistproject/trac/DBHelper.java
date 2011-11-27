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
import java.util.Map;

import org.entomologistproject.trac.EntomologistDatabase;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.provider.BaseColumns;
import android.util.Log;

public class DBHelper {
	private final EntomologistDatabase dbHelp;
	private final SQLiteDatabase db;
	public static final String PREFS_NAME = "EntomologistPreferences";

	DBHelper(Context context) {
		dbHelp = new EntomologistDatabase(context);
		db = dbHelp.getWritableDatabase();
	}

	public void close()
	{
		db.close();
	}
	
	public Cursor getTracker()
	{
		String[] columns = {BaseColumns._ID, "url", "username", "password", "last_updated" };
		Cursor c = db.query("trackers", columns, null, null, null, null, null);
		return c;
	}
	
	public void deleteAttachments(long trackerId, long bugId)
	{
		String[] whereArgs = new String[2];
		whereArgs[0] = String.valueOf(trackerId);
		whereArgs[1] = String.valueOf(bugId);
		db.delete("attachments", "tracker_id=? AND bug_id=?", whereArgs);
	}
	
	public void deleteComments(long trackerId, long bugId)
	{
		String[] whereArgs = new String[2];
		whereArgs[0] = String.valueOf(trackerId);
		whereArgs[1] = String.valueOf(bugId);
		db.delete("comments", "tracker_id=? AND bug_id=?", whereArgs);
	}
	
	public void insertComment(long trackerId,
							  long bugId,
							  String commenter,
							  String comment,
							  Date date)
	{
		ContentValues vals = new ContentValues();
		vals.put("tracker_id", trackerId);
		vals.put("bug_id", bugId);
		vals.put("author", commenter);
		vals.put("comment", comment);
		SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm+0000");
		vals.put("timestamp", sdf.format(date));
		db.insert("comments", null, vals);
	}
	
	public void insertAttachment(long trackerId,
			 					 long bugId,
			 					 String fileName,
			 					 String description,
			 					 String size,
			 					 Date timestamp,
			 					 String submitter)
	{
		ContentValues vals = new ContentValues();
		vals.put("tracker_id", trackerId);
		vals.put("bug_id", bugId);
		vals.put("filename", fileName);
		vals.put("description", description);
		vals.put("size", size);
		vals.put("submitter", submitter);
		SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm+0000");
		vals.put("timestamp", sdf.format(timestamp));
		db.insert("attachments", null, vals);
	}
	
	public void deleteBug(long trackerId, long bugId)
	{
		String[] whereArgs = new String[2];
		whereArgs[0] = String.valueOf(trackerId);
		whereArgs[1] = String.valueOf(bugId);
		db.delete("bugs", "tracker_id=? AND bug_id=?", whereArgs);
	}
	
	public void insertBug(long trackerId, long bugId, Map<String, Object> details)
	{
		ContentValues vals = new ContentValues();
		vals.put("tracker_id", trackerId);
		vals.put("bug_id", bugId);
		if (details.containsKey("ui_notice"))
			vals.put("ui_notice", (String) details.get("ui_notice"));
		if (details.containsKey("type"))
			vals.put("severity", (String) details.get("type"));
		if (details.containsKey("reporter"))
			vals.put("reporter", (String) details.get("reporter"));
		if (details.containsKey("priority"))
			vals.put("priority", (String) details.get("priority"));
		if (details.containsKey("owner"))
			vals.put("assigned_to", (String) details.get("owner"));
		if (details.containsKey("status"))
			vals.put("status", (String) details.get("status"));
		if (details.containsKey("component"))
			vals.put("component", (String) details.get("component"));
		if (details.containsKey("milestone"))
			vals.put("milestone", (String) details.get("milestone"));
		if (details.containsKey("version"))
			vals.put("version", (String) details.get("version"));
		if (details.containsKey("resolution"))
			vals.put("resolution", (String) details.get("resolution"));
		if (details.containsKey("bug_type"))
			vals.put("bug_type", (String) details.get("bug_type"));
		if (details.containsKey("description"))
			vals.put("description", (String) details.get("description"));
		else
			vals.put("description", "<i>No description</i>");
		if (details.containsKey("summary"))
			vals.put("summary", (String) details.get("summary"));
		if (details.containsKey("last_modified"))
			vals.put("last_modified", (String) details.get("last_modified"));
		db.insert("bugs", null, vals);
	}
	
	public long insertTracker(String url, String username, String password)
	{
		ContentValues vals = new ContentValues();
		vals.put("url", url);
		vals.put("username", username);
		vals.put("password", password);
		vals.put("last_updated", "1970-01-01T12:01:00");
		long id = db.insert("trackers", null, vals);
		return id;
	}
	
	public void updateTracker(long id, String url, String username, String password)
	{
		ContentValues vals = new ContentValues();
		vals.put("url", url);
		vals.put("username", username);
		vals.put("password", password);
		vals.put("last_updated", "1970-01-01T12:01:00");
		String[] whereArgs = new String[1];
		whereArgs[0] = String.valueOf(id);
		db.update("trackers", vals, "tracker_id=?", whereArgs);
	}
	
	public void updateTrackerLastUpdated(long id, String modifiedTime)
	{
		ContentValues vals = new ContentValues();
		vals.put("last_updated", modifiedTime);
		String[] whereArgs = new String[1];
		whereArgs[0] = String.valueOf(id);
		db.update("trackers", vals, BaseColumns._ID + "=?", whereArgs);
	}
	
	public void clearFields(long trackerId)
	{
		String[] whereArgs = new String[1];
		whereArgs[0] = String.valueOf(trackerId);
		db.delete("fields", "tracker_id=?", whereArgs);
	}
	
	public void clearAllUiNotices()
	{
		ContentValues vals = new ContentValues();
		vals.put("ui_notice", "");
		db.update("bugs", vals, null, null);
	}
	public void clearUiNotice(long trackerId, long bugId)
	{
		ContentValues vals = new ContentValues();
		vals.put("ui_notice", "");
		String[] whereArgs = new String[2];
		whereArgs[0] = String.valueOf(trackerId);
		whereArgs[1] = String.valueOf(bugId);
		db.update("bugs", vals, "tracker_id = ? AND bug_id = ?", whereArgs);
	}
	public String[] getField(long trackerId, String fieldName)
	{
		String[] ret = null;
		if (trackerId < 0)
			return null;
		
		String[] columns = {"value"};
		String[] where = new String[2];
		where[0] = String.valueOf(trackerId);
		where[1] = fieldName;
		
		Cursor c = db.query("fields", 
				columns, "tracker_id = ? AND field_name = ?", where, null, null, null);
		int sz = c.getCount();

		if (sz == 0)
			return null;		
		ret = new String[sz];
		int i = 0;
		while (c.moveToNext())
		{
			ret[i] = c.getString(0);
			i++;
		}
		
		return ret;
	}
	
	public void insertFields(long trackerId, String fieldName, String[] fieldValues)
	{
		if (fieldValues == null)
			return;
		
		ContentValues vals = new ContentValues();
		for (int i = 0; i < fieldValues.length; ++i)
		{
			//Log.d("Entomologist", "Inserting " + fieldName + " : " + fieldValues[i]);
			vals.put("tracker_id", trackerId);
			vals.put("field_name", fieldName);
			vals.put("value", fieldValues[i]);
			db.insert("fields", null, vals);
		}
	}

	public Bug getBug(long trackerId, long bugId)
	{
		String[] columns = {BaseColumns._ID,
							"severity",
							"priority",
							"assigned_to",
							"status",
							"summary",
							"component",
							"milestone",
							"version",
							"resolution",
							"description",
							"last_modified",
							"reporter"
		};
		String[] whereArgs = new String[2];
		whereArgs[0] = String.valueOf(trackerId);
		whereArgs[1] = String.valueOf(bugId);
		Cursor c = db.query("bugs", columns, "tracker_id = ? AND bug_id = ?", whereArgs, null, null, null);

		if (!c.moveToFirst())
			return null;
		
		Bug item = new Bug();
		item.setSeverity(c.getString(1));
		item.setPriority(c.getString(2));
		item.setAssignedTo(c.getString(3));
		item.setStatus(c.getString(4));
		item.setSummary(c.getString(5));
		item.setComponent(c.getString(6));
		item.setMilestone(c.getString(7));
		item.setVersion(c.getString(8));
		item.setResolution(c.getString(9));
		item.setDescription(c.getString(10));
		item.setLastModified(c.getString(11));
		item.setReportedBy(c.getString(12));
		c.close();
		return item;
	}
	
	public Cursor getComments(long trackerId, long bugId)
	{
		String[] columns = {BaseColumns._ID, "author", "timestamp", "comment"};
		String[] whereArgs = new String[2];
		whereArgs[0] = String.valueOf(trackerId);
		whereArgs[1] = String.valueOf(bugId);
		Cursor c = db.query("comments", columns, "tracker_id = ? AND bug_id = ?", whereArgs, null, null, null);
		return c;
	}
	
	public Cursor getAttachments(long trackerId, long bugId)
	{
		String[] columns = {BaseColumns._ID, "filename", "description", "size", "timestamp", "submitter"};
		String[] whereArgs = new String[2];
		whereArgs[0] = String.valueOf(trackerId);
		whereArgs[1] = String.valueOf(bugId);
		Cursor c = db.query("attachments", columns, "tracker_id = ? AND bug_id = ?", whereArgs, null, null, null);
		return c;
	}
	
	public Cursor getBugs()
	{
		return getBugs(null);
	}
	
	public Cursor getBugs(String sort)
	{
		String[] columns = {BaseColumns._ID, "bug_id", "summary", "component", "status", "last_modified", "ui_notice"};
		Cursor c = db.query("bugs", 
				columns, null, null, null, null, sort);
		return c;
	}
	
	public void setTracker(String url, String username, String password)
	{
		
	}
}
