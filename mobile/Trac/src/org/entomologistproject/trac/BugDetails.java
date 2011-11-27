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

import greendroid.app.GDTabActivity;
import greendroid.widget.ActionBar;
import greendroid.widget.NormalActionBarItem;
import greendroid.widget.ActionBarItem;
import greendroid.widget.QuickAction;
import greendroid.widget.QuickActionBar;
import greendroid.widget.QuickActionWidget;
import greendroid.widget.QuickActionWidget.OnQuickActionClickListener;
import java.util.List;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Configuration;
import android.database.Cursor;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.text.Html;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View.OnClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.SimpleCursorAdapter;
import android.widget.Spinner;
import android.widget.Toast;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.TableRow;
import android.widget.TextView;

// This is the per-bug activity
public class BugDetails extends GDTabActivity {
	private static DBHelper dbHelper;
	private static Bug bug;
	private static long trackerId;
	private static long bugId;
	private boolean noInternet;
	private static String[] trackerComponents;
	private static String[] trackerMilestones;
	private static String[] trackerPriorities;
	private static String[] trackerResolutions;
	private static String[] trackerSeverities;
	private static String[] trackerStatuses;
	private static String[] trackerVersions;
	private static String trackerUrl;
	private static String trackerUsername;
	private static String trackerPassword;
	private static View summaryTabView;
	private static View detailsTabView;
	private Context context;

	private class ChangesUploadedHandler extends Handler {
		final ProgressDialog progressDialog;
		public ChangesUploadedHandler(ProgressDialog pd)
		{
			progressDialog = pd;
		}
		@Override
		public void handleMessage(Message msg)
		{
			Bundle b = msg.getData();
			progressDialog.dismiss();

			if (b.getBoolean("error"))
			{
				Toast toast = Toast.makeText(getApplicationContext(), b.getString("errormsg"), Toast.LENGTH_LONG);
				toast.show();
			}
			else
			{
		        bug = dbHelper.getBug(trackerId, bugId);
				Toast toast = Toast.makeText(getApplicationContext(), "Bug updated", Toast.LENGTH_LONG);
				toast.show();
				
			}
		}

	}
	
	private class CommentsHandler extends Handler {
		final ProgressDialog progressDialog;
		
		public CommentsHandler(ProgressDialog pd)
		{
			progressDialog = pd;
		}
		
		@Override
		public void handleMessage(Message msg)
		{
			progressDialog.dismiss();

			Bundle b = msg.getData();
			if (b.getBoolean("error"))
			{
				Toast toast = Toast.makeText(getApplicationContext(), b.getString("errormsg"), Toast.LENGTH_LONG);
				toast.show();
			}
			
		    Intent newIntent = new Intent(context, CommentsActivity.class);
		    addTab("comments", "Comments", newIntent);
		}
	}

	@Override
	public void onDestroy()
	{
		super.onDestroy();
		dbHelper.close();
	}
	
	public void setupUI()
	{
        getActionBar().setType(ActionBar.Type.Empty);
        setTitle("Bug #" + bugId);
        
        addActionBarItem(getActionBar()
                .newActionBarItem(NormalActionBarItem.class)
                .setDrawable(R.drawable.ic_menu_upload),
                R.id.action_bar_upload);
        
        Intent newIntent = new Intent(this, SummaryActivity.class);
        addTab("summary", "Summary", newIntent);
        newIntent = new Intent(this, DetailsActivity.class);
        addTab("details", "Details", newIntent);
	}
	
	// This prevents the comment sync on a screen rotation
	@Override
	public void onConfigurationChanged(Configuration newConfig)
	{
	    super.onConfigurationChanged(newConfig);
	}
	
	public void onCreate(Bundle savedInstanceState) {
	    super.onCreate(savedInstanceState);
	    context = this;
	    setContentView(R.layout.bug_detail_view);

        Bundle extras = getIntent().getExtras(); 
        bugId = extras.getLong("bug_id");
        trackerId = extras.getLong("tracker_id");
        noInternet = extras.getBoolean("no_internet", false);
        trackerUrl = extras.getString("tracker_url");
        trackerUsername = extras.getString("tracker_username");
        trackerPassword = extras.getString("tracker_password");

    	trackerComponents = extras.getStringArray("components");
    	trackerMilestones = extras.getStringArray("milestones");
    	trackerPriorities = extras.getStringArray("priorities");
    	trackerResolutions = extras.getStringArray("resolutions");
    	trackerSeverities = extras.getStringArray("severities");
    	trackerStatuses = extras.getStringArray("statuses");
    	trackerVersions = extras.getStringArray("versions");
        dbHelper = new DBHelper(this);
        bug = dbHelper.getBug(trackerId, bugId);
        
        if (bug == null)
        {
        	finish();
        }
	    setupUI();
	    
	    if (!noInternet)
	    {
	    	TracBackend backend = new TracCommentsTask(this, 
	    											   trackerId,
	    											   trackerUrl,
	    											   trackerUsername,
	    											   trackerPassword,
	    											   bugId);
	    	backend.execute(new CommentsHandler(ProgressDialog.show(BugDetails.this, "", 
	    			"Retrieving comments...", true)));
	    }
	    else
	    {
			Toast toast = Toast.makeText(getApplicationContext(),
										 "No internet - operating in cached mode.",
										 Toast.LENGTH_SHORT);
			toast.show();

		    Intent newIntent = new Intent(context, CommentsActivity.class);
		    addTab("comments", "Comments", newIntent);
	    }

	}

    @Override
    public boolean onHandleActionBarItemClick(ActionBarItem item, int position) {
        switch (item.getItemId())
        {
        case R.id.action_bar_upload:
        	uploadChanges();
        	break;
        default:
        	return super.onHandleActionBarItemClick(item, position);
        }
        return true;
    }
	/*
	private void loadCommentsAndFiles()
	{      
		Cursor attachmentCursor = dbHelper.getAttachments(trackerId, bugId);
		String[] attachmentsFrom = {"filename", "description", "size", "timestamp", "submitter"};
		int[] attachmentsTo = { R.id.attachmentFilename,
								R.id.attachmentDescription,
								R.id.attachmentSize,
								R.id.attachmentTimestamp,
								R.id.attachmentSubmitter};
		SimpleCursorAdapter attachmentsAdapter = new SimpleCursorAdapter(BugDetails.this,
															R.layout.attachment_row,
															attachmentCursor, attachmentsFrom, attachmentsTo);
		ListView attachmentView = (ListView) findViewById(R.id.attachmentList);
		attachmentView.setAdapter(attachmentsAdapter);
        attachmentView.setOnItemClickListener(new OnItemClickListener() {
            public void onItemClick(AdapterView<?> parent, View view,
                    int position, long id) {
            			TextView fileNameTextView = (TextView) view.findViewById(R.id.attachmentFilename);
            			String fileName = fileNameTextView.getText().toString();
            			String fileNameArray[] = fileName.split("\\.");
            	        String ext = fileNameArray[fileNameArray.length-1];
            	        String mimeType;
            			MimeTypeMap map = MimeTypeMap.getSingleton();
            	        if (ext.equals(fileName))
            	        	mimeType = "text/plain";
            	        else
            	        	mimeType = map.getMimeTypeFromExtension(ext);
            			launchAttachmentHandler(fileName, mimeType);
                }
              });

	} */
    
	@SuppressWarnings("unused")
	private void launchAttachmentHandler(String fileName, String mimeType)
	{
		Intent newIntent = new Intent("android.intent.action.VIEW");
		newIntent.setType(mimeType);
		PackageManager pm = getPackageManager();
		List<ResolveInfo> list = pm.queryIntentActivities(newIntent, android.content.pm.PackageManager.MATCH_DEFAULT_ONLY);
		final String[] spinnerArray = new String[list.size()];
		for (int i = 0; i < list.size(); ++i) {
			ResolveInfo info = list.get(i);
			spinnerArray[0] = info.loadLabel(pm).toString();
		}
		
		// Now ask the user if they really want to open this file
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle("Open attachment with...");
		builder.setItems(spinnerArray, new DialogInterface.OnClickListener() {
		    public void onClick(DialogInterface dialog, int item) {
		        Toast.makeText(getApplicationContext(), spinnerArray[item], Toast.LENGTH_SHORT).show();
		    }
		});
		AlertDialog alert = builder.create();
		alert.show();

	}
	
	private void uploadChanges()
	{
		Bundle b = new Bundle();
		String changelogText = "<b>Change:</b><br><br><hr>";
		String text = TextViewToString(R.id.statusId, summaryTabView);
		if (bug.getStatus() != null && !bug.getStatus().equals(text))
		{
			b.putString("status", text);
			if (text == "")
				text = "<i>None</i>";
			changelogText += changelogLine("Status", text);
		}
		text = TextViewToString(R.id.resolutionId, summaryTabView);
		if (bug.getResolution() != null && !bug.getResolution().equals(text))
		{
			b.putString("resolution", text);
			if (text == "")
				text = "<i>None</i>";

			changelogText += changelogLine("Resolution", text);
		}
		
		text = TextViewToString(R.id.reportedById, summaryTabView); 
		if (bug.getReportedBy() != null && !bug.getReportedBy().equals(text))
		{
			b.putString("reporter", text);
			if (text == "")
				text = "<i>None</i>";

			changelogText += changelogLine("Reporter", text);
		}
		
		text = TextViewToString(R.id.assignedToEdit, summaryTabView);
		if (bug.getAssignedTo() != null && !bug.getAssignedTo().equals(text))
		{
			b.putString("owner", text);
			if (text == "")
				text = "<i>None</i>";

			changelogText += changelogLine("Assigned To", text);
		}
		
		text = TextViewToString(R.id.summaryId, summaryTabView);
		if (bug.getSummary() != null && !bug.getSummary().equals(text))
		{
			b.putString("summary", text);
			if (text == "")
				text = "<i>None</i>";

			changelogText += changelogLine("Summary", text);
		}
		
		text = SpinnerToString(R.id.typeSpinner, detailsTabView, trackerSeverities);
		if (bug.getSeverity() != null && text != null && !bug.getSeverity().equals(text))
		{
			b.putString("severity", text);
			if (text == "")
				text = "<i>None</i>";

			changelogText += changelogLine("Severity", text);
		}
		
		text = SpinnerToString(R.id.prioritySpinner, detailsTabView, trackerPriorities);
		if (bug.getPriority() != null && text != null && !bug.getPriority().equals(text))
		{
			b.putString("priority", text);
			if (text == "")
				text = "<i>None</i>";

			changelogText += changelogLine("Priority", text);
		}
		
		text = SpinnerToString(R.id.versionSpinner, detailsTabView, trackerVersions);
		if (bug.getVersion() != null&& text != null && !bug.getVersion().equals(text))
		{
			b.putString("version", text);
			if (text == "")
				text = "<i>None</i>";

			changelogText += changelogLine("Version", text);
		}
		
		text = SpinnerToString(R.id.milestoneSpinner, detailsTabView, trackerMilestones);
		if (bug.getMilestone() != null && text != null && !bug.getMilestone().equals(text))
		{
			b.putString("milestone", text);
			if (text == "")
				text = "<i>None</i>";
			changelogText += changelogLine("Milestone", text);
		}
		
		text = SpinnerToString(R.id.componentSpinner, detailsTabView, trackerComponents);
		if (bug.getComponent() != null && text != null && !bug.getComponent().equals(text))
		{
			b.putString("component", text);
			if (text == "")
				text = "<i>None</i>";

			changelogText += changelogLine("Component", text);
		}
		
		if (b.size() == 0)
			return;
		
		b.putLong("bug_id", bugId);
		b.putLong("tracker_id", trackerId);
		final Bundle submitBundle = new Bundle(b);
		// Now show the changelog and give the user the chance to back out
		AlertDialog.Builder changelogDialog = new AlertDialog.Builder(this);
		changelogDialog.setInverseBackgroundForced(true);
		changelogDialog.setIcon(R.drawable.icon);
		changelogDialog.setMessage("Are you sure you want to upload the following changes?");
		changelogDialog.setTitle("Changelog");
		changelogDialog.setCancelable(false);
		
		View v = LayoutInflater.from(this).inflate(R.layout.changelog_layout, null);
		TextView changelogView = (TextView) v.findViewById(R.id.changelogTextView);
		changelogView.setText(Html.fromHtml(changelogText));
		changelogDialog.setView(v);
		  
		changelogDialog.setPositiveButton("Yes", 
		    new android.content.DialogInterface.OnClickListener() {
		     public void onClick(DialogInterface dialog, int arg1) {
					TracBackend backend = new TracUploadChanges(context, trackerUrl,
							trackerUsername, trackerPassword, submitBundle);
					backend.execute(new ChangesUploadedHandler(ProgressDialog.show(BugDetails.this, "", 
							"Uploading changes...", true)));		     }
		    }
		   );

		changelogDialog.setNegativeButton("No", 
			    new android.content.DialogInterface.OnClickListener() {
			     public void onClick(DialogInterface dialog, int arg1) {
			     }
			    }
			   );

		changelogDialog.show();
	}
	
	static private String changelogLine(String field, String newValue)
	{
		String ret = "<b>" + field + "</b> to <font color=red>" + newValue + "</font><br>";
		return ret;
	}
	
	static private String SpinnerToString(int id, View view, String[] vals)
	{
		if (view == null)
			return null;
		Spinner spinner = (Spinner) view.findViewById(id);
		if (spinner == null)
			return null;
		
		int index = spinner.getSelectedItemPosition();
		if (index < 0)
			return null;
		
		return vals[index];
	}
	
	static private String TextViewToString(int id, View view)
	{
		TextView text = (TextView) view.findViewById(id);
		if (text == null)
			return "";
		
		return text.getText().toString();
	}
    
    public static class SummaryActivity extends Activity
    {
      	private QuickActionWidget statusBar;
    	private QuickActionWidget summaryBar;
    	private QuickActionWidget assignedToBar;
    	private QuickActionWidget reportedBar;
    	@Override
    	public void onConfigurationChanged(Configuration newConfig)
    	{
    		super.onConfigurationChanged(newConfig);
    		//Log.v("Entomologist", "Summary changed config");
    	}
    	 @Override
    	 protected void onCreate(Bundle savedInstanceState) {
    		 super.onCreate(savedInstanceState);
    		 //Log.v("Entomologist", "SummaryActivity onCreate");
    		 setContentView(R.layout.summary_tab_view);
    		 summaryTabView = findViewById(R.id.summaryScrollView);
    		 summaryBar = new QuickActionBar(this);
    		 summaryBar.addQuickAction(new QuickAction(this, R.drawable.gd_action_bar_edit, R.string.gd_edit));
    		 summaryBar.setOnQuickActionClickListener(summaryListener);

    		 reportedBar = new QuickActionBar(this);
    		 reportedBar.addQuickAction(new QuickAction(this, R.drawable.gd_action_bar_edit, R.string.gd_edit));
    		 reportedBar.setOnQuickActionClickListener(reporterListener);

    		 assignedToBar = new QuickActionBar(this);
    		 assignedToBar.addQuickAction(new QuickAction(this, R.drawable.gd_action_bar_edit, R.string.gd_edit));
    		 assignedToBar.setOnQuickActionClickListener(assignedToListener);

    		 statusBar = new QuickActionBar(this);
    		 statusBar.addQuickAction(new QuickAction(this, R.drawable.gd_action_bar_edit, R.string.gd_edit));
    		 statusBar.setOnQuickActionClickListener(statusListener);

    		 TextView view = (TextView) findViewById(R.id.reportedById);
    		 view.setText(bug.getReportedBy());
    		 view = (TextView) findViewById(R.id.statusId);
    		 view.setText(bug.getStatus());
    		 String resolution = bug.getResolution();
			 view = (TextView) findViewById(R.id.resolutionId);
			 view.setText(resolution);
    		 
    		 view = (TextView) findViewById(R.id.summaryId);
    		 view.setText(bug.getSummary());
    		 view = (TextView) findViewById(R.id.descriptionId);
    		 view.setText(bug.getDescription());
    		 view = (TextView) findViewById(R.id.assignedToEdit);
    		 view.setText(bug.getAssignedTo());
    	 }
    	 
    	 public void statusClick(View view)
    	 {
    		 statusBar.show(view);
    	 }
    	 
    	 public void reportedClick(View view)
    	 {
    		 reportedBar.show(view);
    	 }

    	 public void assignedToClick(View view)
    	 {
    		 assignedToBar.show(view);
    	 }

    	 public void summaryClick(View view)
    	 {
    		 summaryBar.show(view);
    	 }
    	 
    	 private void showStatusDialog()
    	 {
			 TextView status = (TextView) findViewById(R.id.statusId);
			 TextView resolution = (TextView) findViewById(R.id.resolutionId);
			 final StatusDialog dialog = new StatusDialog(this, 
					 								trackerStatuses, trackerResolutions, status, resolution);
			 dialog.show();

    	 }
    	 
    	 private OnQuickActionClickListener statusListener = new OnQuickActionClickListener() {
    		 public void onQuickActionClicked(QuickActionWidget widget, int position) {
    			 showStatusDialog();
    		 }
    	 };
    	 
    	 private OnQuickActionClickListener summaryListener = new OnQuickActionClickListener() {
    		 public void onQuickActionClicked(QuickActionWidget widget, int position) {
        		 TextView view = (TextView) findViewById(R.id.summaryId);
        		 String currentText = view.getText().toString();
        		 getNewValue("Editing Summary", currentText, true, view);
    		 }
    	 };

    	 private OnQuickActionClickListener reporterListener = new OnQuickActionClickListener() {
    		 public void onQuickActionClicked(QuickActionWidget widget, int position) {
        		 TextView reporterView = (TextView) findViewById(R.id.reportedById);
        		 String currentText = reporterView.getText().toString();
        		 getNewValue("Editing Reporter", currentText, false, reporterView);
    		 }
    	 };

    	 private OnQuickActionClickListener assignedToListener = new OnQuickActionClickListener() {
    		 public void onQuickActionClicked(QuickActionWidget widget, int position) {
        		 TextView assignedView = (TextView) findViewById(R.id.assignedToEdit);
        		 String currentText = assignedView.getText().toString();
        		 getNewValue("Editing Assigned To", currentText, false, assignedView);
    		 }
    	 };
    	 
    	 public void getNewValue(String title, String currentText, boolean multiLine, TextView tv)
    	 {
    		 AlertDialog.Builder alert = new AlertDialog.Builder(this);
    		 alert.setTitle(title);

    		 final EditText input = new EditText(this);
    		 final TextView textView = tv;
    		 if (multiLine)
    		 {
    			 input.setInputType(android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_FLAG_MULTI_LINE);
    			 input.setMinLines(3);
    			 input.setGravity(Gravity.TOP);
    		 }
    		 else
    		 {
    			 input.setInputType(android.text.InputType.TYPE_CLASS_TEXT);

    		 }
    		 input.setText(currentText);
    		 alert.setView(input);
    		 alert.setPositiveButton("OK", new DialogInterface.OnClickListener() {
    			 public void onClick(DialogInterface dialog, int whichButton) {
    				 String val = input.getText().toString();
    				 if (val.length() > 0)
    					 textView.setText(val);
    			 }
    		 });

    		 alert.show();
    	 }

    }
    

    public static class  CommentsActivity extends Activity
    {
    	private EditText newCommentEdit;
    	private Button newCommentSubmit;
    	private Cursor listCursor;
    	private SimpleCursorAdapter adapter;
    	
    	@Override
    	protected void onDestroy()
    	{
    		super.onDestroy();
    		listCursor.close();
    	}
    	
    	@Override
         protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
         	 setContentView(R.layout.comment_tab_view);
     		listCursor = dbHelper.getComments(trackerId, bugId);
    		String[] from = {"author", "timestamp", "comment"};
    		int[] to = { R.id.commenter, R.id.commentDate, R.id.comment };
    		adapter = new SimpleCursorAdapter(CommentsActivity.this,
    															R.layout.comment_row,
    															listCursor, from, to);
    		ListView v = (ListView) findViewById(R.id.commentList);
    		v.setAdapter(adapter);
    		newCommentEdit = (EditText) findViewById(R.id.newCommentEditText);
    		newCommentEdit.setVisibility(View.GONE);
    		newCommentSubmit = (Button) findViewById(R.id.newCommentSave);
    		newCommentSubmit.setVisibility(View.GONE);
    		newCommentSubmit.setOnClickListener(new OnClickListener() {
				@Override
				public void onClick(View v) {
					String newComment = newCommentEdit.getText().toString();
					TracBackend backend = new TracUploadComment(getApplicationContext(), trackerUrl,
										trackerUsername, trackerPassword, newComment, trackerId, bugId);
					backend.execute(new CommentUploadHandler(ProgressDialog.show(CommentsActivity.this, "", 
							"Uploading comment...", true)));
				}
    			
    		});
         }
    	 private class CommentUploadHandler extends Handler {
    		 final ProgressDialog progressDialog;

    		 public CommentUploadHandler(ProgressDialog pd)
    		 {
    			 progressDialog = pd;
    		 }

    		 @Override
    		 public void handleMessage(Message msg)
    		 {
    			 progressDialog.dismiss();

    			 Bundle b = msg.getData();
    			 if (b.getBoolean("error"))
    			 {
    				 Toast toast = Toast.makeText(getApplicationContext(),
    						 			b.getString("errormsg"), Toast.LENGTH_LONG);
    				 toast.show();
    			 }
    			 if (b.getBoolean("comment_uploaded"))
    			 {
    				 listCursor.requery();
    				 newCommentEdit.setText("");
    				 toggleNewComment(null);
    				 InputMethodManager imm = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
    				 imm.hideSoftInputFromWindow(newCommentEdit.getWindowToken(), 0);
    				 Toast toast = Toast.makeText(getApplicationContext(),
					 			"Success!", Toast.LENGTH_LONG);
    				 toast.show();
    			 }
    		 }
    	 }

    	 public void toggleNewComment(View view)
    	 {
    		 if (newCommentEdit.isShown())
    		 {
    			 newCommentEdit.setVisibility(View.GONE);
    			 newCommentSubmit.setVisibility(View.GONE);
    		 }
    		 else
    		 {
    			 newCommentEdit.setVisibility(View.VISIBLE);
    			 newCommentSubmit.setVisibility(View.VISIBLE);
    		 }
    	 }
    }

    public static class DetailsActivity extends Activity
    {
    	 @Override
         protected void onCreate(Bundle savedInstanceState) {
             super.onCreate(savedInstanceState);
         	 setContentView(R.layout.details_tab_view);
         	 detailsTabView = findViewById(R.id.detailsTabView);
         	 if (trackerComponents == null)
         		 hideRow(R.id.componentRow);
         	 else
         		 setSpinner(R.id.componentSpinner, trackerComponents, bug.getComponent());

         	 if (trackerMilestones == null)
         		 hideRow(R.id.milestoneRow);
         	 else
         		 setSpinner(R.id.milestoneSpinner, trackerMilestones, bug.getMilestone());

         	 if (trackerPriorities == null)
         		 hideRow(R.id.priorityRow);
         	 else
         		 setSpinner(R.id.prioritySpinner, trackerPriorities, bug.getPriority());

         	 if (trackerSeverities == null)
         		 hideRow(R.id.typeRow);
         	 else
         		 setSpinner(R.id.typeSpinner, trackerSeverities, bug.getSeverity());

         	 if (trackerVersions == null)
         		 hideRow(R.id.versionRow);
         	 else
         		 setSpinner(R.id.versionSpinner, trackerVersions, bug.getVersion());
    	 }
    	 
    	private void setSpinner(int id, String[] list, String selected)
    	{
    			Spinner spinner = (Spinner) findViewById(id);
    			ArrayAdapter<String> spinnerAdapter = new ArrayAdapter<String>(this, 
    					R.layout.spinner_view, R.id.spinnerText,
    									list);
    			spinner.setAdapter(spinnerAdapter);
    			spinner.setSelection(spinnerAdapter.getPosition(selected));
    	}
    	
    	private void hideRow(int id)
    	{
    		TableRow row = (TableRow) findViewById(id);
    		row.setVisibility(android.view.View.GONE);
    	}
    }
}
