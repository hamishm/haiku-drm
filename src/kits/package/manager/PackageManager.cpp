/*
 * Copyright 2013, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ingo Weinhold <ingo_weinhold@gmx.de>
 */


#include <package/manager/PackageManager.h>

#include <Directory.h>
#include <package/PackageRoster.h>
#include <package/RepositoryCache.h>
#include <package/solver/SolverPackage.h>
#include <package/solver/SolverPackageSpecifier.h>
#include <package/solver/SolverPackageSpecifierList.h>
#include <package/solver/SolverProblem.h>
#include <package/solver/SolverProblemSolution.h>
#include <package/solver/SolverResult.h>

#include <CopyEngine.h>
#include <package/ActivationTransaction.h>
#include <package/DaemonClient.h>
#include <package/manager/RepositoryBuilder.h>

#include "PackageManagerUtils.h"


namespace BPackageKit {

namespace BManager {

namespace BPrivate {


// #pragma mark - BPackageManager


BPackageManager::BPackageManager(BPackageInstallationLocation location)
	:
	fLocation(location),
	fSolver(NULL),
	fSystemRepository(new (std::nothrow) InstalledRepository("system",
		B_PACKAGE_INSTALLATION_LOCATION_SYSTEM, -1)),
	fCommonRepository(new (std::nothrow) InstalledRepository("common",
		B_PACKAGE_INSTALLATION_LOCATION_COMMON, -2)),
	fHomeRepository(new (std::nothrow) InstalledRepository("home",
		B_PACKAGE_INSTALLATION_LOCATION_HOME, -3)),
	fInstalledRepositories(10),
	fOtherRepositories(10, true),
	fTransactions(5, true),
	fInstallationInterface(NULL),
	fRequestHandler(NULL),
	fUserInteractionHandler(NULL)
{
}


BPackageManager::~BPackageManager()
{
	delete fSolver;
	delete fSystemRepository;
	delete fCommonRepository;
	delete fHomeRepository;
}


void
BPackageManager::Init(uint32 flags)
{
	if (fSolver != NULL)
		return;

	// create the solver
	status_t error = BSolver::Create(fSolver);
	if (error != B_OK)
		DIE(error, "failed to create solver");

	if (fSystemRepository == NULL || fCommonRepository == NULL
		|| fHomeRepository == NULL) {
		throw std::bad_alloc();
	}

	// add installation location repositories
	if ((flags & B_ADD_INSTALLED_REPOSITORIES) != 0) {
		// We add only the repository of our actual installation location as the
		// "installed" repository. The repositories for the more general
		// installation locations are added as regular repositories, but with
		// better priorities than the actual (remote) repositories. This
		// prevents the solver from showing conflicts when a package in a more
		// specific installation location overrides a package in a more general
		// one. Instead any requirement that is already installed in a more
		// general installation location will turn up as to be installed as
		// well. But we can easily filter those out.
		_AddInstalledRepository(fSystemRepository);

		if (!fSystemRepository->IsInstalled()) {
			_AddInstalledRepository(fCommonRepository);

			if (!fCommonRepository->IsInstalled())
				_AddInstalledRepository(fHomeRepository);
		}
	}

	// add other repositories
	if ((flags & B_ADD_REMOTE_REPOSITORIES) != 0) {
		BPackageRoster roster;
		BStringList repositoryNames;
		error = roster.GetRepositoryNames(repositoryNames);
		if (error != B_OK) {
			fUserInteractionHandler->Warn(error,
				"failed to get repository names");
		}

		int32 repositoryNameCount = repositoryNames.CountStrings();
		for (int32 i = 0; i < repositoryNameCount; i++) {
			_AddRemoteRepository(roster, repositoryNames.StringAt(i),
				(flags & B_REFRESH_REPOSITORIES) != 0);
		}
	}
}


void
BPackageManager::Install(const char* const* packages, int packageCount)
{
	BSolverPackageSpecifierList packagesToInstall;
	if (!packagesToInstall.AppendSpecifiers(packages, packageCount))
		throw std::bad_alloc();
	Install(packagesToInstall);
}


void
BPackageManager::Install(const BSolverPackageSpecifierList& packages)
{
	Init(B_ADD_INSTALLED_REPOSITORIES | B_ADD_REMOTE_REPOSITORIES
		| B_REFRESH_REPOSITORIES);

	// solve
	const BSolverPackageSpecifier* unmatchedSpecifier;
	status_t error = fSolver->Install(packages, &unmatchedSpecifier);
	if (error != B_OK) {
		if (unmatchedSpecifier != NULL) {
			DIE(error, "failed to find a match for \"%s\"",
				unmatchedSpecifier->SelectString().String());
		} else
			DIE(error, "failed to compute packages to install");
	}

	_HandleProblems();

	// install/uninstall packages
	_AnalyzeResult();
	_ConfirmChanges();
	_ApplyPackageChanges();
}


void
BPackageManager::Uninstall(const char* const* packages, int packageCount)
{
	BSolverPackageSpecifierList packagesToUninstall;
	if (!packagesToUninstall.AppendSpecifiers(packages, packageCount))
		throw std::bad_alloc();
	Uninstall(packagesToUninstall);
}


void
BPackageManager::Uninstall(const BSolverPackageSpecifierList& packages)
{
	Init(B_ADD_INSTALLED_REPOSITORIES);

	// find the packages that match the specification
	const BSolverPackageSpecifier* unmatchedSpecifier;
	PackageList foundPackages;
	status_t error = fSolver->FindPackages(packages,
		BSolver::B_FIND_INSTALLED_ONLY, foundPackages, &unmatchedSpecifier);
	if (error != B_OK) {
		if (unmatchedSpecifier != NULL) {
			DIE(error, "failed to find a match for \"%s\"",
				unmatchedSpecifier->SelectString().String());
		} else
			DIE(error, "failed to compute packages to uninstall");
	}

	// determine the inverse base package closure for the found packages
// TODO: Optimize!
	InstalledRepository& installationRepository = InstallationRepository();
	bool foundAnotherPackage;
	do {
		foundAnotherPackage = false;
		int32 count = installationRepository.CountPackages();
		for (int32 i = 0; i < count; i++) {
			BSolverPackage* package = installationRepository.PackageAt(i);
			if (foundPackages.HasItem(package))
				continue;

			if (_FindBasePackage(foundPackages, package->Info()) >= 0) {
				foundPackages.AddItem(package);
				foundAnotherPackage = true;
			}
		}
	} while (foundAnotherPackage);

	// remove the packages from the repository
	for (int32 i = 0; BSolverPackage* package = foundPackages.ItemAt(i); i++)
		installationRepository.DisablePackage(package);

	for (;;) {
		error = fSolver->VerifyInstallation(BSolver::B_VERIFY_ALLOW_UNINSTALL);
		if (error != B_OK)
			DIE(error, "failed to compute packages to uninstall");

		_HandleProblems();

		// (virtually) apply the result to this repository
		_AnalyzeResult();

		for (int32 i = foundPackages.CountItems() - 1; i >= 0; i--) {
			if (!installationRepository.PackagesToDeactivate()
					.AddItem(foundPackages.ItemAt(i))) {
				throw std::bad_alloc();
			}
		}

		installationRepository.ApplyChanges();

		// verify the next specific respository
		if (!_NextSpecificInstallationLocation())
			break;

		foundPackages.MakeEmpty();

		// NOTE: In theory, after verifying a more specific location, it would
		// be more correct to compute the inverse base package closure for the
		// packages we need to uninstall and (if anything changed) verify again.
		// In practice, however, base packages are always required with an exact
		// version (ATM). If that base package still exist in a more general
		// location (the only reason why the package requiring the base package
		// wouldn't be marked to be uninstalled as well) there shouldn't have
		// been any reason to remove it from the more specific location in the
		// first place.
	}

	_ConfirmChanges(true);
	_ApplyPackageChanges(true);
}


void
BPackageManager::Update(const char* const* packages, int packageCount)
{
	BSolverPackageSpecifierList packagesToUpdate;
	if (!packagesToUpdate.AppendSpecifiers(packages, packageCount))
		throw std::bad_alloc();
	Update(packagesToUpdate);
}


void
BPackageManager::Update(const BSolverPackageSpecifierList& packages)
{
	Init(B_ADD_INSTALLED_REPOSITORIES | B_ADD_REMOTE_REPOSITORIES
		| B_REFRESH_REPOSITORIES);

	// solve
	const BSolverPackageSpecifier* unmatchedSpecifier;
	status_t error = fSolver->Update(packages, true,
		&unmatchedSpecifier);
	if (error != B_OK) {
		if (unmatchedSpecifier != NULL) {
			DIE(error, "failed to find a match for \"%s\"",
				unmatchedSpecifier->SelectString().String());
		} else
			DIE(error, "failed to compute packages to update");
	}

	_HandleProblems();

	// install/uninstall packages
	_AnalyzeResult();
	_ConfirmChanges();
	_ApplyPackageChanges();
}


void
BPackageManager::VerifyInstallation()
{
	Init(B_ADD_INSTALLED_REPOSITORIES | B_ADD_REMOTE_REPOSITORIES
		| B_REFRESH_REPOSITORIES);

	for (;;) {
		status_t error = fSolver->VerifyInstallation();
		if (error != B_OK)
			DIE(error, "failed to compute package dependencies");

		_HandleProblems();

		// (virtually) apply the result to this repository
		_AnalyzeResult();
		InstallationRepository().ApplyChanges();

		// verify the next specific respository
		if (!_NextSpecificInstallationLocation())
			break;
	}

	_ConfirmChanges();
	_ApplyPackageChanges();
}


BPackageManager::InstalledRepository&
BPackageManager::InstallationRepository()
{
	if (fInstalledRepositories.IsEmpty())
		DIE("no installation repository");

	return *fInstalledRepositories.LastItem();
}


void
BPackageManager::_HandleProblems()
{
	while (fSolver->HasProblems()) {
		fUserInteractionHandler->HandleProblems();

		status_t error = fSolver->SolveAgain();
		if (error != B_OK)
			DIE(error, "failed to recompute packages to un/-install");
	}
}


void
BPackageManager::_AnalyzeResult()
{
	BSolverResult result;
	status_t error = fSolver->GetResult(result);
	if (error != B_OK)
		DIE(error, "failed to compute packages to un/-install");

	InstalledRepository& installationRepository = InstallationRepository();
	PackageList& packagesToActivate
		= installationRepository.PackagesToActivate();
	PackageList& packagesToDeactivate
		= installationRepository.PackagesToDeactivate();

	PackageList potentialBasePackages;

	for (int32 i = 0; const BSolverResultElement* element = result.ElementAt(i);
			i++) {
		BSolverPackage* package = element->Package();

		switch (element->Type()) {
			case BSolverResultElement::B_TYPE_INSTALL:
			{
				PackageList& packageList
					= dynamic_cast<InstalledRepository*>(package->Repository())
							!= NULL
						? potentialBasePackages
						: packagesToActivate;
				if (!packageList.AddItem(package))
					throw std::bad_alloc();
				break;
			}

			case BSolverResultElement::B_TYPE_UNINSTALL:
				if (!packagesToDeactivate.AddItem(package))
					throw std::bad_alloc();
				break;
		}
	}

	// Make sure base packages are installed in the same location.
	for (int32 i = 0; i < packagesToActivate.CountItems(); i++) {
		BSolverPackage* package = packagesToActivate.ItemAt(i);
		int32 index = _FindBasePackage(potentialBasePackages, package->Info());
		if (index < 0)
			continue;

		BSolverPackage* basePackage = potentialBasePackages.RemoveItemAt(index);
		if (!packagesToActivate.AddItem(basePackage))
			throw std::bad_alloc();
	}

	fInstallationInterface->ResultComputed(installationRepository);
}


void
BPackageManager::_ConfirmChanges(bool fromMostSpecific)
{
	// check, if there are any changes at all
	int32 count = fInstalledRepositories.CountItems();
	bool hasChanges = false;
	for (int32 i = 0; i < count; i++) {
		if (fInstalledRepositories.ItemAt(i)->HasChanges()) {
			hasChanges = true;
			break;
		}
	}

	if (!hasChanges)
		throw BNothingToDoException();

	fUserInteractionHandler->ConfirmChanges(fromMostSpecific);
}


void
BPackageManager::_ApplyPackageChanges(bool fromMostSpecific)
{
	int32 count = fInstalledRepositories.CountItems();
	if (fromMostSpecific) {
		for (int32 i = count - 1; i >= 0; i--)
			_PreparePackageChanges(*fInstalledRepositories.ItemAt(i));
	} else {
		for (int32 i = 0; i < count; i++)
			_PreparePackageChanges(*fInstalledRepositories.ItemAt(i));
	}

	for (int32 i = 0; Transaction* transaction = fTransactions.ItemAt(i); i++)
		_CommitPackageChanges(*transaction);

// TODO: Clean up the transaction directories on error!
}


void
BPackageManager::_PreparePackageChanges(
	InstalledRepository& installationRepository)
{
	if (!installationRepository.HasChanges())
		return;

	PackageList& packagesToActivate
		= installationRepository.PackagesToActivate();
	PackageList& packagesToDeactivate
		= installationRepository.PackagesToDeactivate();

	// create the transaction
	Transaction* transaction = new Transaction(installationRepository);
	if (!fTransactions.AddItem(transaction)) {
		delete transaction;
		throw std::bad_alloc();
	}

	status_t error = fInstallationInterface->PrepareTransaction(*transaction);
	if (error != B_OK)
		DIE(error, "failed to create transaction");

	// download the new packages and prepare the transaction
	for (int32 i = 0; BSolverPackage* package = packagesToActivate.ItemAt(i);
		i++) {
		// get package URL and target entry

		BString fileName(package->Info().FileName());
		if (fileName.IsEmpty())
			throw std::bad_alloc();

		BEntry entry;
		error = entry.SetTo(&transaction->TransactionDirectory(), fileName);
		if (error != B_OK)
			DIE(error, "failed to create package entry");

		RemoteRepository* remoteRepository
			= dynamic_cast<RemoteRepository*>(package->Repository());
		if (remoteRepository == NULL) {
			// clone the existing package (unless already present)
			if (package->Repository() != &installationRepository) {
				_ClonePackageFile(
					dynamic_cast<InstalledRepository*>(package->Repository()),
					fileName, entry);
			}
		} else {
			// download the package
			BString url = remoteRepository->Config().PackagesURL();
			url << '/' << fileName;

			status_t error = fRequestHandler->DownloadPackage(url, entry,
				package->Info().Checksum());
			if (error != B_OK)
				DIE(error, "failed to download package");
		}

		// add package to transaction
		if (!transaction->ActivationTransaction().AddPackageToActivate(
				fileName)) {
			throw std::bad_alloc();
		}
	}

	for (int32 i = 0; BSolverPackage* package = packagesToDeactivate.ItemAt(i);
		i++) {
		// add package to transaction
		if (!transaction->ActivationTransaction().AddPackageToDeactivate(
				package->Info().FileName())) {
			throw std::bad_alloc();
		}
	}
}


void
BPackageManager::_CommitPackageChanges(Transaction& transaction)
{
	InstalledRepository& installationRepository = transaction.Repository();

	fUserInteractionHandler->ProgressStartApplyingChanges(
		installationRepository);

	// commit the transaction
	BDaemonClient::BCommitTransactionResult transactionResult;
	status_t error = fInstallationInterface->CommitTransaction(transaction,
		transactionResult);
	if (error != B_OK)
		DIE(error, "failed to commit transaction");
	if (transactionResult.Error() != B_OK) {
		DIE("failed to commit transaction: %s",
			transactionResult.FullErrorMessage().String());
	}

	fUserInteractionHandler->ProgressTransactionCommitted(
		installationRepository, transactionResult.OldStateDirectory());

	BEntry transactionDirectoryEntry;
	if ((error = transaction.TransactionDirectory()
			.GetEntry(&transactionDirectoryEntry)) != B_OK
		|| (error = transactionDirectoryEntry.Remove()) != B_OK) {
		fUserInteractionHandler->Warn(error,
			"failed to remove transaction directory");
	}

	fUserInteractionHandler->ProgressApplyingChangesDone(
		installationRepository);
}


void
BPackageManager::_ClonePackageFile(InstalledRepository* repository,
	const BString& fileName, const BEntry& entry)
{
	// get the source and destination file paths
	directory_which packagesWhich;
	if (repository == fSystemRepository) {
		packagesWhich = B_SYSTEM_PACKAGES_DIRECTORY;
	} else if (repository == fCommonRepository) {
		packagesWhich = B_COMMON_PACKAGES_DIRECTORY;
	} else {
		DIE("don't know packages directory path for installation location "
			"\"%s\"", repository->Name().String());
	}

	BPath sourcePath;
	status_t error = find_directory(packagesWhich, &sourcePath);
	if (error != B_OK || (error = sourcePath.Append(fileName)) != B_OK) {
		DIE(error, "failed to get path of package file \"%s\" in installation "
			"location \"%s\"", fileName.String(), repository->Name().String());
	}

	BPath destinationPath;
	error = entry.GetPath(&destinationPath);
	if (error != B_OK) {
		DIE(error, "failed to entry path of package file to install \"%s\"",
			fileName.String());
	}

	// Copy the package. Ideally we would just hard-link it, but BFS doesn't
	// support that.
	error = BCopyEngine().CopyEntry(sourcePath.Path(), destinationPath.Path());
	if (error != B_OK)
		DIE(error, "failed to copy package file \"%s\"", sourcePath.Path());
}


int32
BPackageManager::_FindBasePackage(const PackageList& packages,
	const BPackageInfo& info)
{
	if (info.BasePackage().IsEmpty())
		return -1;

	// find the requirement matching the base package
	BPackageResolvableExpression* basePackage = NULL;
	int32 count = info.RequiresList().CountItems();
	for (int32 i = 0; i < count; i++) {
		BPackageResolvableExpression* requires = info.RequiresList().ItemAt(i);
		if (requires->Name() == info.BasePackage()) {
			basePackage = requires;
			break;
		}
	}

	if (basePackage == NULL) {
		fUserInteractionHandler->Warn(B_OK, "package %s-%s doesn't have a "
			"matching requires for its base package \"%s\"",
			info.Name().String(), info.Version().ToString().String(),
			info.BasePackage().String());
		return -1;
	}

	// find the first package matching the base package requires
	count = packages.CountItems();
	for (int32 i = 0; i < count; i++) {
		BSolverPackage* package = packages.ItemAt(i);
		if (package->Name() == basePackage->Name()
			&& package->Info().Matches(*basePackage)) {
			return i;
		}
	}

	return -1;
}


void
BPackageManager::_AddInstalledRepository(InstalledRepository* repository)
{
	fInstallationInterface->InitInstalledRepository(*repository);

	BRepositoryBuilder(*repository)
		.AddToSolver(fSolver, repository->Location() == fLocation);
	repository->SetPriority(repository->InitialPriority());

	if (!fInstalledRepositories.AddItem(repository))
		throw std::bad_alloc();
}


void
BPackageManager::_AddRemoteRepository(BPackageRoster& roster, const char* name,
	bool refresh)
{
	BRepositoryConfig config;
	status_t error = roster.GetRepositoryConfig(name, &config);
	if (error != B_OK) {
		fUserInteractionHandler->Warn(error,
			"failed to get config for repository \"%s\". Skipping.", name);
		return;
	}

	BRepositoryCache cache;
	error = _GetRepositoryCache(roster, config, refresh, cache);
	if (error != B_OK) {
		fUserInteractionHandler->Warn(error,
			"failed to get cache for repository \"%s\". Skipping.", name);
		return;
	}

	RemoteRepository* repository = new RemoteRepository(config);
	if (!fOtherRepositories.AddItem(repository)) {
		delete repository;
		throw std::bad_alloc();
	}

	BRepositoryBuilder(*repository, cache, config.Name())
		.AddToSolver(fSolver, false);
}


status_t
BPackageManager::_GetRepositoryCache(BPackageRoster& roster,
	const BRepositoryConfig& config, bool refresh, BRepositoryCache& _cache)
{
	if (!refresh && roster.GetRepositoryCache(config.Name(), &_cache) == B_OK)
		return B_OK;

	status_t error = fRequestHandler->RefreshRepository(config);
	if (error != B_OK) {
		fUserInteractionHandler->Warn(error,
			"refreshing repository \"%s\" failed", config.Name().String());
	}

	return roster.GetRepositoryCache(config.Name(), &_cache);
}


bool
BPackageManager::_NextSpecificInstallationLocation()
{
	if (fLocation == B_PACKAGE_INSTALLATION_LOCATION_SYSTEM) {
		fLocation = B_PACKAGE_INSTALLATION_LOCATION_COMMON;
		fSystemRepository->SetInstalled(false);
		_AddInstalledRepository(fCommonRepository);
		return true;
	}

	if (fLocation == B_PACKAGE_INSTALLATION_LOCATION_COMMON) {
		fLocation = B_PACKAGE_INSTALLATION_LOCATION_HOME;
		fCommonRepository->SetInstalled(false);
		_AddInstalledRepository(fHomeRepository);
		return true;
	}

	return false;
}


// #pragma mark - RemoteRepository


BPackageManager::RemoteRepository::RemoteRepository(
	const BRepositoryConfig& config)
	:
	BSolverRepository(),
	fConfig(config)
{
}


const BRepositoryConfig&
BPackageManager::RemoteRepository::Config() const
{
	return fConfig;
}


// #pragma mark - InstalledRepository


BPackageManager::InstalledRepository::InstalledRepository(const char* name,
	BPackageInstallationLocation location, int32 priority)
	:
	BSolverRepository(),
	fDisabledPackages(10, true),
	fPackagesToActivate(),
	fPackagesToDeactivate(),
	fInitialName(name),
	fLocation(location),
	fInitialPriority(priority)
{
}


void
BPackageManager::InstalledRepository::DisablePackage(BSolverPackage* package)
{
	if (fDisabledPackages.HasItem(package))
		DIE("package %s already disabled", package->VersionedName().String());

	if (package->Repository() != this) {
		DIE("package %s not in repository %s",
			package->VersionedName().String(), Name().String());
	}

	// move to disabled list
	if (!fDisabledPackages.AddItem(package))
		throw std::bad_alloc();

	RemovePackage(package);
}


bool
BPackageManager::InstalledRepository::EnablePackage(BSolverPackage* package)
{
	return fDisabledPackages.RemoveItem(package);
}


bool
BPackageManager::InstalledRepository::HasChanges() const
{
	return !fPackagesToActivate.IsEmpty() || !fPackagesToDeactivate.IsEmpty();
}


void
BPackageManager::InstalledRepository::ApplyChanges()
{
	// disable packages to deactivate
	for (int32 i = 0; BSolverPackage* package = fPackagesToDeactivate.ItemAt(i);
		i++) {
		if (!fDisabledPackages.HasItem(package))
			DisablePackage(package);
	}

	// add packages to activate
	for (int32 i = 0; BSolverPackage* package = fPackagesToActivate.ItemAt(i);
		i++) {
		status_t error = AddPackage(package->Info());
		if (error != B_OK) {
			DIE(error, "failed to add package %s to %s repository",
				package->Name().String(), Name().String());
		}
	}
}


// #pragma mark - Transaction


BPackageManager::Transaction::Transaction(InstalledRepository& repository)
	:
	fRepository(repository),
	fTransaction(),
	fTransactionDirectory()
{
}


BPackageManager::Transaction::~Transaction()
{
}


// #pragma mark - InstallationInterface


BPackageManager::InstallationInterface::~InstallationInterface()
{
}


void
BPackageManager::InstallationInterface::ResultComputed(
	InstalledRepository& repository)
{
}


// #pragma mark - ClientInstallationInterface


BPackageManager::ClientInstallationInterface::ClientInstallationInterface()
	:
	fDaemonClient()
{
}


BPackageManager::ClientInstallationInterface::~ClientInstallationInterface()
{
}


void
BPackageManager::ClientInstallationInterface::InitInstalledRepository(
	InstalledRepository& repository)
{
	const char* name = repository.InitialName();
	BRepositoryBuilder(repository, name)
		.AddPackages(repository.Location(), name);
}


status_t
BPackageManager::ClientInstallationInterface::PrepareTransaction(
	Transaction& transaction)
{
	return fDaemonClient.CreateTransaction(transaction.Repository().Location(),
		transaction.ActivationTransaction(),
		transaction.TransactionDirectory());
}


status_t
BPackageManager::ClientInstallationInterface::CommitTransaction(
	Transaction& transaction, BDaemonClient::BCommitTransactionResult& _result)
{
	return fDaemonClient.CommitTransaction(transaction.ActivationTransaction(),
		_result);
}


// #pragma mark - RequestHandler


BPackageManager::RequestHandler::~RequestHandler()
{
}


// #pragma mark - UserInteractionHandler


BPackageManager::UserInteractionHandler::~UserInteractionHandler()
{
}


}	// namespace BPrivate

}	// namespace BManager

}	// namespace BPackageKit