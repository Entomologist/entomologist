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

public class Bug {
	private String bugNumber;
	private String severity;
	private String priority;
	private String assignedTo;
	private String reportedBy;
	private String status;
	private String summary;
	private String component;
	private String milestone;
	private String version;
	private String resolution;
	private String description;
	private String lastModified;
	
	// We keep track of bug modifications here to simplify things
	private String modifiedSeverity;
	private String modifiedPriority;
	private String modifiedAssignedTo;
	private String modifiedReportedBy;
	private String modifiedStatus;
	private String modifiedSummary;
	private String modifiedComponent;
	private String modifiedMilestone;
	private String modifiedVersion;
	private String modifiedResolution;
	private String modifiedDescription;
	private int isModified;
	public Bug() {
		isModified = 0;
		milestone = "";
		severity = "";
		priority ="";
		assignedTo = "";
		reportedBy = "";
		status = "";
		summary = "";
		component = "";
		version = "";
		resolution = "";
		description = "";
		lastModified = "";
		modifiedSeverity = ""; 
		modifiedPriority = "";
		modifiedAssignedTo = "";
		modifiedReportedBy = "";
		modifiedStatus = "";
		modifiedSummary = "";
		modifiedComponent = "";
		modifiedMilestone = "";
		modifiedVersion = "";
		modifiedResolution = "";
		modifiedDescription = "";
	}

	public String getBugNumber() {
		return bugNumber;
	}
	public void setBugNumber(String bugNumber) {
		this.bugNumber = bugNumber;
	}
	public String getSeverity() {
		return severity;
	}
	public void setSeverity(String severity) {
		this.severity = severity;
	}
	public String getPriority() {
		return priority;
	}
	public void setPriority(String priority) {
		this.priority = priority;
	}
	public String getAssignedTo() {
		return assignedTo;
	}
	public void setAssignedTo(String assignedTo) {
		this.assignedTo = assignedTo;
	}
	public String getReportedBy() {
		return reportedBy;
	}
	public void setReportedBy(String reportedBy) {
		this.reportedBy = reportedBy;
	}
	public String getStatus() {
		return status;
	}
	public void setStatus(String status) {
		this.status = status;
	}
	public String getSummary() {
		return summary;
	}
	public void setSummary(String summary) {
		this.summary = summary;
	}
	public String getComponent() {
		return component;
	}
	public void setComponent(String component) {
		this.component = component;
	}
	public String getMilestone() {
		return milestone;
	}
	public void setMilestone(String milestone) {
		this.milestone = milestone;
	}
	public String getVersion() {
		return version;
	}
	public void setVersion(String version) {
		this.version = version;
	}
	public String getResolution() {
		return resolution;
	}
	public void setResolution(String resolution) {
		this.resolution = resolution;
	}
	public String getDescription() {
		return description;
	}
	public void setDescription(String description) {
		this.description = description;
	}
	public String getLastModified() {
		return lastModified;
	}
	public void setLastModified(String lastModified) {
		this.lastModified = lastModified;
	}

	public String getModifiedSeverity() {
		return modifiedSeverity;
	}

	public void setModifiedSeverity(String modifiedSeverity) {
		isModified++;
		this.modifiedSeverity = modifiedSeverity;
	}

	public String getModifiedPriority() {
		return modifiedPriority;
	}

	public void setModifiedPriority(String modifiedPriority) {
		isModified++;
		this.modifiedPriority = modifiedPriority;
	}

	public String getModifiedAssignedTo() {
		return modifiedAssignedTo;
	}

	public void setModifiedAssignedTo(String modifiedAssignedTo) {
		isModified++;
		this.modifiedAssignedTo = modifiedAssignedTo;
	}

	public String getModifiedReportedBy() {
		return modifiedReportedBy;
	}

	public void setModifiedReportedBy(String modifiedReportedBy) {
		isModified++;
		this.modifiedReportedBy = modifiedReportedBy;
	}

	public String getModifiedStatus() {
		return modifiedStatus;
	}

	public void setModifiedStatus(String modifiedStatus) {
		isModified++;
		this.modifiedStatus = modifiedStatus;
	}

	public String getModifiedSummary() {
		return modifiedSummary;
	}

	public void setModifiedSummary(String modifiedSummary) {
		isModified++;
		this.modifiedSummary = modifiedSummary;
	}

	public String getModifiedComponent() {
		return modifiedComponent;
	}

	public void setModifiedComponent(String modifiedComponent) {
		isModified++;
		this.modifiedComponent = modifiedComponent;
	}

	public String getModifiedMilestone() {
		return modifiedMilestone;
	}

	public void setModifiedMilestone(String modifiedMilestone) {
		isModified++;
		this.modifiedMilestone = modifiedMilestone;
	}

	public String getModifiedVersion() {
		return modifiedVersion;
	}

	public void setModifiedVersion(String modifiedVersion) {
		isModified++;
		this.modifiedVersion = modifiedVersion;
	}

	public String getModifiedResolution() {
		return modifiedResolution;
	}

	public void setModifiedResolution(String modifiedResolution) {
		isModified++;
		this.modifiedResolution = modifiedResolution;
	}

	public String getModifiedDescription() {
		return modifiedDescription;
	}

	public void setModifiedDescription(String modifiedDescription) {
		isModified++;
		this.modifiedDescription = modifiedDescription;
	}

	public int isModified()
	{
		return isModified;
	}
}
